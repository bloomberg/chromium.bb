// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_parsing_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace u2f_parsing_utils {

namespace {
constexpr uint8_t kOne[] = {0x01};
constexpr uint8_t kTwoThree[] = {0x02, 0x03};
constexpr uint8_t kThree[] = {0x03};
constexpr uint8_t kOneTwoThree[] = {0x01, 0x02, 0x03};
}  // namespace

TEST(U2fParsingUtils, Materialize) {
  const std::vector<uint8_t> empty;
  EXPECT_THAT(Materialize(empty), ::testing::IsEmpty());
  EXPECT_THAT(Materialize(base::span<const uint8_t>()), ::testing::IsEmpty());

  EXPECT_THAT(Materialize(kOne), ::testing::ElementsAreArray(kOne));
  EXPECT_THAT(Materialize(kOneTwoThree),
              ::testing::ElementsAreArray(kOneTwoThree));
}

TEST(U2fParsingUtils, Append) {
  std::vector<uint8_t> target;

  Append(&target, base::span<const uint8_t>());
  EXPECT_THAT(target, ::testing::IsEmpty());

  // Should be idempotent, try twice for good measure.
  Append(&target, base::span<const uint8_t>());
  EXPECT_THAT(target, ::testing::IsEmpty());

  const std::vector<uint8_t> one(std::begin(kOne), std::end(kOne));
  Append(&target, one);
  EXPECT_THAT(target, ::testing::ElementsAreArray(kOne));

  Append(&target, kTwoThree);
  EXPECT_THAT(target, ::testing::ElementsAreArray(kOneTwoThree));
}

// ExtractSpan and ExtractSuffixSpan are implicitly tested as they used by
// the Extract and ExtractSuffix implementations.

TEST(U2fParsingUtils, ExtractEmpty) {
  const std::vector<uint8_t> empty;
  EXPECT_THAT(Extract(empty, 0, 0), ::testing::IsEmpty());

  EXPECT_THAT(Extract(kOne, 0, 0), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOne, 1, 0), ::testing::IsEmpty());

  EXPECT_THAT(Extract(kOneTwoThree, 0, 0), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 1, 0), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 2, 0), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 3, 0), ::testing::IsEmpty());
}

TEST(U2fParsingUtils, ExtractInBounds) {
  EXPECT_THAT(Extract(kOne, 0, 1), ::testing::ElementsAreArray(kOne));
  EXPECT_THAT(Extract(kOneTwoThree, 0, 1), ::testing::ElementsAreArray(kOne));
  EXPECT_THAT(Extract(kOneTwoThree, 2, 1), ::testing::ElementsAreArray(kThree));
  EXPECT_THAT(Extract(kOneTwoThree, 1, 2),
              ::testing::ElementsAreArray(kTwoThree));
  EXPECT_THAT(Extract(kOneTwoThree, 0, 3),
              ::testing::ElementsAreArray(kOneTwoThree));
}

TEST(U2fParsingUtils, ExtractOutOfBounds) {
  const std::vector<uint8_t> empty;
  EXPECT_THAT(Extract(empty, 0, 1), ::testing::IsEmpty());
  EXPECT_THAT(Extract(empty, 1, 0), ::testing::IsEmpty());

  EXPECT_THAT(Extract(kOne, 0, 2), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOne, 1, 1), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOne, 2, 0), ::testing::IsEmpty());

  EXPECT_THAT(Extract(kOneTwoThree, 0, 4), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 1, 3), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 2, 2), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 3, 1), ::testing::IsEmpty());
  EXPECT_THAT(Extract(kOneTwoThree, 4, 0), ::testing::IsEmpty());
}

TEST(U2fParsingUtils, ExtractSuffixEmpty) {
  const std::vector<uint8_t> empty;
  EXPECT_THAT(ExtractSuffix(empty, 0), ::testing::IsEmpty());
  EXPECT_THAT(ExtractSuffix(kOne, 1), ::testing::IsEmpty());
  EXPECT_THAT(ExtractSuffix(kOneTwoThree, 3), ::testing::IsEmpty());
}

TEST(U2fParsingUtils, ExtractSuffixInBounds) {
  EXPECT_THAT(ExtractSuffix(kOne, 0), ::testing::ElementsAreArray(kOne));
  EXPECT_THAT(ExtractSuffix(kOneTwoThree, 1),
              ::testing::ElementsAreArray(kTwoThree));
  EXPECT_THAT(ExtractSuffix(kOneTwoThree, 2),
              ::testing::ElementsAreArray(kThree));
}

TEST(U2fParsingUtils, ExtractSuffixOutOfBounds) {
  const std::vector<uint8_t> empty;
  EXPECT_THAT(ExtractSuffix(empty, 1), ::testing::IsEmpty());
  EXPECT_THAT(ExtractSuffix(kOne, 2), ::testing::IsEmpty());
  EXPECT_THAT(ExtractSuffix(kOneTwoThree, 4), ::testing::IsEmpty());
}

}  // namespace u2f_parsing_utils
}  // namespace device
