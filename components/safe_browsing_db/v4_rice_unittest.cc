// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_rice.h"
#include "testing/platform_test.h"

using ::google::protobuf::RepeatedField;
using ::google::protobuf::int32;

namespace safe_browsing {

class V4RiceTest : public PlatformTest {
 public:
  V4RiceTest() {}
};

TEST_F(V4RiceTest, TestDecoderGetNextWordWithNoData) {
  uint32_t word;
  V4RiceDecoder decoder(5, 1, "");
  EXPECT_EQ(DECODE_RAN_OUT_OF_BITS_FAILURE, decoder.GetNextWord(&word));
}

TEST_F(V4RiceTest, TestDecoderGetNextBitsWithNoData) {
  uint32_t word;
  V4RiceDecoder decoder(5, 1, "");
  EXPECT_EQ(DECODE_RAN_OUT_OF_BITS_FAILURE, decoder.GetNextBits(1, &word));
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithNoData) {
  uint32_t word;
  V4RiceDecoder decoder(5, 1, "");
  EXPECT_EQ(DECODE_RAN_OUT_OF_BITS_FAILURE, decoder.GetNextValue(&word));
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithNoEntries) {
  uint32_t word;
  V4RiceDecoder decoder(28, 0, "\xbf\xa8");
  EXPECT_FALSE(decoder.HasAnotherValue());
  EXPECT_EQ(DECODE_NO_MORE_ENTRIES_FAILURE, decoder.GetNextValue(&word));
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithSmallValues) {
  uint32_t word;
  V4RiceDecoder decoder(2, 2, "\xf7\x2");
  EXPECT_EQ(DECODE_SUCCESS, decoder.GetNextValue(&word));
  EXPECT_EQ(15u, word);
  EXPECT_EQ(DECODE_SUCCESS, decoder.GetNextValue(&word));
  EXPECT_EQ(9u, word);
  EXPECT_FALSE(decoder.HasAnotherValue());
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithLargeValues) {
  uint32_t word;
  V4RiceDecoder decoder(28, 3,
                        "\xbf\xa8\x3f\xfb\xfc\xfb\x5e\x27\xe6\xc3\x1d\xc6\x38");
  EXPECT_EQ(DECODE_SUCCESS, decoder.GetNextValue(&word));
  EXPECT_EQ(1777762129u, word);
  EXPECT_EQ(DECODE_SUCCESS, decoder.GetNextValue(&word));
  EXPECT_EQ(2093280223u, word);
  EXPECT_EQ(DECODE_SUCCESS, decoder.GetNextValue(&word));
  EXPECT_EQ(924369848u, word);
  EXPECT_FALSE(decoder.HasAnotherValue());
}

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderIntegersWithNoData) {
  RepeatedField<int32> out;
  EXPECT_EQ(ENCODED_DATA_UNEXPECTED_EMPTY_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, 5, 1, "", &out));
}
#endif

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderIntegersWithNegativeNumEntries) {
  RepeatedField<int32> out;
  EXPECT_EQ(NUM_ENTRIES_NEGATIVE_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, 5, -1, "", &out));
}
#endif

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderIntegersWithNonPositiveRiceParameter) {
  RepeatedField<int32> out;
  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, 0, 1, "a", &out));

  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, -1, 1, "a", &out));
}
#endif

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderIntegersWithOverflowValues) {
  RepeatedField<int32> out;
  EXPECT_EQ(DECODED_INTEGER_OVERFLOW_FAILURE,
            V4RiceDecoder::DecodeIntegers(
                5, 28, 3,
                "\xbf\xa8\x3f\xfb\xfc\xfb\x5e\x27\xe6\xc3\x1d\xc6\x38", &out));
}
#endif

TEST_F(V4RiceTest, TestDecoderIntegersWithOneValue) {
  RepeatedField<int32> out;
  EXPECT_EQ(DECODE_SUCCESS, V4RiceDecoder::DecodeIntegers(3, 2, 0, "", &out));
  EXPECT_EQ(1, out.size());
  EXPECT_EQ(3, out.Get(0));
}

TEST_F(V4RiceTest, TestDecoderIntegersWithMultipleValues) {
  RepeatedField<int32> out;
  EXPECT_EQ(DECODE_SUCCESS,
            V4RiceDecoder::DecodeIntegers(5, 2, 2, "\xf7\x2", &out));
  EXPECT_EQ(3, out.size());
  EXPECT_EQ(5, out.Get(0));
  EXPECT_EQ(20, out.Get(1));
  EXPECT_EQ(29, out.Get(2));
}

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderBytesWithNoData) {
  std::string out;
  EXPECT_EQ(ENCODED_DATA_UNEXPECTED_EMPTY_FAILURE,
            V4RiceDecoder::DecodeBytes(3, 5, 1, "", &out));
}
#endif

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderBytesWithNegativeNumEntries) {
  std::string out;
  EXPECT_EQ(NUM_ENTRIES_NEGATIVE_FAILURE,
            V4RiceDecoder::DecodeBytes(3, 5, -1, "", &out));
}
#endif

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderBytesWithNonPositiveRiceParameter) {
  std::string out;
  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodeBytes(3, 0, 1, "a", &out));

  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodeBytes(3, -1, 1, "a", &out));
}
#endif

TEST_F(V4RiceTest, TestDecoderBytesWithOneValue) {
  std::string out;
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(DECODE_SUCCESS,
            V4RiceDecoder::DecodeBytes(0x69F67F51u, 2, 0, "", &out));
  EXPECT_EQ(4u, out.size());
  EXPECT_EQ("Q\x7F\xF6i", out);
}

TEST_F(V4RiceTest, TestDecoderBytesWithOneSmallValue) {
  std::string out;
  EXPECT_EQ(DECODE_SUCCESS, V4RiceDecoder::DecodeBytes(17u, 2, 0, "", &out));
  EXPECT_EQ(4u, out.size());
  EXPECT_EQ(std::string("\x11\0\0\0", 4), out);
}

TEST_F(V4RiceTest, TestDecoderBytesWithMultipleValues) {
  std::string out;
  EXPECT_EQ(DECODE_SUCCESS,
            V4RiceDecoder::DecodeBytes(
                5, 28, 3, "\xbf\xa8\x3f\xfb\xf\xf\x5e\x27\xe6\xc3\x1d\xc6\x38",
                &out));
  EXPECT_EQ(16u, out.size());
  EXPECT_EQ(std::string("\x5\0\0\0V\x7F\xF6o\xCEo1\x81\fL\x93\xAD", 16), out);
}

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// This test hits a NOTREACHED so it is a release mode only test.
TEST_F(V4RiceTest, TestDecoderBytesWithOverflowValues) {
  std::string out;
  EXPECT_EQ(DECODED_INTEGER_OVERFLOW_FAILURE,
            V4RiceDecoder::DecodeBytes(
                5, 28, 3,
                "\xbf\xa8\x3f\xfb\xfc\xfb\x5e\x27\xe6\xc3\x1d\xc6\x38", &out));
}
#endif

}  // namespace safe_browsing
