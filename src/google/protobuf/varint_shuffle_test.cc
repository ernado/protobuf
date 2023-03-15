// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "google/protobuf/varint_shuffle.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Must be included last.
#include "google/protobuf/port_def.inc"

using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Range;
using testing::TestWithParam;

namespace google {
namespace protobuf {
namespace internal {
namespace {

int64_t ToInt64(char c) { return static_cast<uint8_t>(c); }
int32_t ToInt32(char c) { return static_cast<uint8_t>(c); }

int NaiveParse32(const char* p, int32_t& res) {
  int len = 0;
  res = ToInt32(*p);
  while (*p++ & 0x80) {
    if (++len == 10) return 11;
    if (len < 5) res += (ToInt32(*p) - 1) << (len * 7);
  }
  return ++len;
}

// A naive, easy to verify implementation for test purposes.
int NaiveParse64(const char* p, int64_t& res) {
  int len = 0;
  res = ToInt64(*p);
  while (*p++ & 0x80) {
    if (++len == 10) return 11;
    res += (ToInt64(*p) - 1) << (len * 7);
  }
  return ++len;
}

// A naive, easy to verify implementation for test purposes.
int NaiveSerialize(char* p, uint64_t value) {
  int n = 0;
  while (value > 127) {
    p[n++] = 0x80 | static_cast<char>(value);
    value >>= 7;
  }
  p[n++] = static_cast<char>(value);
  return n;
}

class ShiftMixParseVarint32Test : public TestWithParam<int> {
 public:
  int length() const { return GetParam(); }
};
class ShiftMixParseVarint64Test : public TestWithParam<int> {
 public:
  int length() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(Default, ShiftMixParseVarint32Test, Range(1, 11));
INSTANTIATE_TEST_SUITE_P(Default, ShiftMixParseVarint64Test, Range(1, 11));

template <int limit = 10>
const char* Parse(const char* data, int32_t& res) {
  int64_t res64;
  const char* ret = ShiftMixParseVarint<int32_t, limit>(data, res64);
  res = res64;
  return ret;
}

template <int limit = 10>
const char* Parse(const char* data, int64_t& res) {
  return ShiftMixParseVarint<int64_t, limit>(data, res);
}

template <int limit = 0>
const char* ParseWithLimit(int rtlimit, const char* data, int32_t& res) {
  if (rtlimit > limit) return ParseWithLimit<limit + 1>(rtlimit, data, res);
  return Parse<limit>(data, res);
}

template <int limit = 0>
const char* ParseWithLimit(int rtlimit, const char* data, int64_t& res) {
  if (rtlimit > limit) return ParseWithLimit<limit + 1>(rtlimit, data, res);
  return Parse<limit>(data, res);
}

template <>
const char* ParseWithLimit<10>(int rtlimit, const char* data, int32_t& res) {
  return Parse<10>(data, res);
}

template <>
const char* ParseWithLimit<10>(int rtlimit, const char* data, int64_t& res) {
  return Parse<10>(data, res);
}

TEST_P(ShiftMixParseVarint32Test, AllLengths) {
  const int len = length();

  std::vector<char> bytes;
  for (int i = 1; i < len; ++i) {
    bytes.push_back(0xC1 + (i << 1));
  }
  bytes.push_back(0x01);
  const char* data = bytes.data();

  int32_t expected;
  ASSERT_THAT(NaiveParse32(data, expected), Eq(len));

  int32_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

TEST_P(ShiftMixParseVarint32Test, NotCanonical) {
  int len = length();

  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x7E};
  if (len < 10) data[len++] = 0;

  int32_t expected;
  ASSERT_THAT(NaiveParse32(data, expected), Eq(len));

  int32_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

TEST_P(ShiftMixParseVarint32Test, NotCanonicalZero) {
  int len = length();

  char data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7E};
  if (len < 10) data[len++] = 0;

  int32_t expected;
  ASSERT_THAT(NaiveParse32(data, expected), Eq(len));
  ASSERT_THAT(expected, Eq(0));

  int32_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

TEST_P(ShiftMixParseVarint64Test, AllLengths) {
  const int len = length();

  std::vector<char> bytes;
  for (int i = 1; i < len; ++i) {
    bytes.push_back(0xC1 + (i << 1));
  }
  bytes.push_back(0x01);
  const char* data = bytes.data();

  int64_t expected;
  ASSERT_THAT(NaiveParse64(data, expected), Eq(len));

  int64_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

TEST_P(ShiftMixParseVarint64Test, NotCanonical) {
  int len = length();

  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x7E};
  if (len < 10) data[len++] = 0;

  int64_t expected;
  ASSERT_THAT(NaiveParse64(data, expected), Eq(len));

  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

TEST_P(ShiftMixParseVarint64Test, NotCanonicalZero) {
  int len = length();

  char data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7E};
  if (len < 10) data[len++] = 0;

  int64_t expected;
  ASSERT_THAT(NaiveParse64(data, expected), Eq(len));
  ASSERT_THAT(expected, Eq(0));

  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

TEST_P(ShiftMixParseVarint64Test, HittingLimit) {
  const int limit = length();

  int64_t res = 0x9897969594939291LL;
  char data[10];
  int serialized_len = NaiveSerialize(data, res);
  ASSERT_THAT(serialized_len, Eq(10));

  int64_t result;
  const char* p = ParseWithLimit(limit, data, result);
  if (limit == 10) {
    ASSERT_THAT(p, testing::NotNull());
    ASSERT_THAT(p - data, Eq(limit));
    ASSERT_THAT(result, Eq(res));
  } else {
    res |= (int64_t{-1} << (limit * 7));
    ASSERT_THAT(p, IsNull());
    ASSERT_THAT(result, Eq(res));
  }
}

TEST_P(ShiftMixParseVarint64Test, AtOrBelowLimit) {
  const int limit = length();

  int64_t res = 0x9897969594939291ULL >> (70 - 7 * limit);
  char data[10];
  int serialized_len = NaiveSerialize(data, res);
  ASSERT_THAT(serialized_len, Eq(limit));

  int64_t result;
  const char* p = ParseWithLimit(limit, data, result);
  ASSERT_THAT(p, testing::NotNull());
  ASSERT_THAT(p - data, Eq(limit));
  ASSERT_THAT(result, Eq(res));
}

TEST(ShiftMixParseVarint64Test, OverLong) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x81};
  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, IsNull());
}

TEST(ShiftMixParseVarint32Test, OverLong) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x81};
  int32_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, IsNull());
}

TEST(ShiftMixParseVarint64SingleTest, IgnoringOverlongBits) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x7F};
  int64_t expected;
  ASSERT_THAT(NaiveParse64(data, expected), Eq(10));

  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(10));
  ASSERT_THAT(result, Eq(expected));
}

TEST(ShiftMixParseVarint32SingleTest, DroppingOverlongBits) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0x7F};
  int32_t expected;
  ASSERT_THAT(NaiveParse32(data, expected), Eq(5));

  int32_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(5));
  ASSERT_THAT(result, Eq(expected));
}

TEST(ShiftMixParseVarintTest, OverLong32) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x81};
  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, IsNull());
}

TEST(ShiftMixParseVarintTest, OverLong64) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x81};
  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, IsNull());
}

TEST(ShiftMixParseVarintTest, IgnoringOverlongBits32) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x7F};
  int32_t expected;
  ASSERT_THAT(NaiveParse32(data, expected), Eq(10));

  int32_t result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(10));
  ASSERT_THAT(result, Eq(expected));
}

TEST(ShiftMixParseVarintTest, IgnoringOverlongBits64) {
  char data[] = {0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf, 0xd1, 0xd3, 0x7F};
  int64_t expected;
  ASSERT_THAT(NaiveParse64(data, expected), Eq(10));

  int64_t result;
  const char* p = ShiftMixParseVarint(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(10));
  ASSERT_THAT(result, Eq(expected));
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
