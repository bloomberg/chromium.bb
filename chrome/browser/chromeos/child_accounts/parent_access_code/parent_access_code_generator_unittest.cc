// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_code_generator.h"

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kTestSharedSecret[] = "AIfVJHITSar8keeq3779V70dWiS1xbPv8g";
constexpr base::TimeDelta kDefaultValidity = base::TimeDelta::FromMinutes(10);
constexpr base::TimeDelta kDefaultClockDrift = base::TimeDelta::FromMinutes(5);

ParentAccessCodeConfig DefaultConfig() {
  return ParentAccessCodeConfig(kTestSharedSecret, kDefaultValidity,
                                kDefaultClockDrift);
}

}  // namespace

class ParentAccessCodeGeneratorTest : public testing::Test {
 protected:
  ParentAccessCodeGeneratorTest() = default;
  ~ParentAccessCodeGeneratorTest() override = default;

  // Validates that |code| is valid for the given |timestamp|.
  void Validate(base::Optional<ParentAccessCode> code, base::Time timestamp) {
    ASSERT_TRUE(code.has_value());
    EXPECT_GE(timestamp, code->valid_from());
    EXPECT_LE(timestamp, code->valid_to());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessCodeGeneratorTest);
};

TEST_F(ParentAccessCodeGeneratorTest, HardcodedCodeValues) {
  // Test generator against Parent Access Code values generated in Family Link
  // Android app.
  std::map<base::Time, std::string> test_values;
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("8 Jan 2019 16:58:07 PST", &timestamp));
  test_values[timestamp] = "734261";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:35:05 PST", &timestamp));
  test_values[timestamp] = "472150";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:42:49 PST", &timestamp));
  test_values[timestamp] = "204984";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:53:01 PST", &timestamp));
  test_values[timestamp] = "157758";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 16:00:00 PST", &timestamp));
  test_values[timestamp] = "524186";

  ParentAccessCodeGenerator gen(DefaultConfig());
  for (const auto& it : test_values) {
    base::Optional<ParentAccessCode> code = gen.Generate(it.first);
    ASSERT_NO_FATAL_FAILURE(Validate(code, it.first));
    EXPECT_EQ(it.second, code->code());
  }
}

TEST_F(ParentAccessCodeGeneratorTest, SameTimeBucket) {
  // Test that the same code is generated for whole time bucket defined by code
  // validity period.
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:00:00 PST", &timestamp));
  const ParentAccessCodeConfig config = DefaultConfig();

  ParentAccessCodeGenerator gen(config);
  base::Optional<ParentAccessCode> first_code = gen.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(first_code, timestamp));

  int steps =
      (config.code_validity() / ParentAccessCodeGenerator::kCodeGranularity) -
      1;
  for (int i = 0; i < steps; ++i) {
    timestamp += ParentAccessCodeGenerator::kCodeGranularity;
    base::Optional<ParentAccessCode> code = gen.Generate(timestamp);
    ASSERT_NO_FATAL_FAILURE(Validate(code, timestamp));
    EXPECT_EQ(*first_code, *code);
  }
}

TEST_F(ParentAccessCodeGeneratorTest, DifferentTimeBuckets) {
  // Test that the different codes are generated for different time buckets
  // defined by code validity period.
  base::Time initial_timestamp;
  ASSERT_TRUE(
      base::Time::FromString("14 Jan 2019 15:00:00 PST", &initial_timestamp));

  ParentAccessCodeGenerator gen(DefaultConfig());
  base::Optional<ParentAccessCode> first_code = gen.Generate(initial_timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(first_code, initial_timestamp));

  for (int i = 1; i < 10; ++i) {
    // "Earlier" time bucket.
    {
      const base::Time timestamp = initial_timestamp - i * kDefaultValidity;
      base::Optional<ParentAccessCode> code = gen.Generate(timestamp);
      ASSERT_NO_FATAL_FAILURE(Validate(code, timestamp));
      EXPECT_NE(*first_code, *code);
    }
    // "Later" time bucket.
    {
      const base::Time timestamp = initial_timestamp + i * kDefaultValidity;
      base::Optional<ParentAccessCode> code = gen.Generate(timestamp);
      ASSERT_NO_FATAL_FAILURE(Validate(code, timestamp));
      EXPECT_NE(*first_code, *code);
    }
  }
}

TEST_F(ParentAccessCodeGeneratorTest, SameTimestamp) {
  // Test that codes generated with the same timestamp and config are the same.
  const ParentAccessCodeConfig config = DefaultConfig();
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeGenerator gen1(config);
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code1, timestamp));

  ParentAccessCodeGenerator gen2(config);
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code2, timestamp));

  EXPECT_EQ(*code1, *code2);
}

TEST_F(ParentAccessCodeGeneratorTest, DifferentSharedSecret) {
  // Test that codes generated with the different secrets are not the same.
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeGenerator gen1(ParentAccessCodeConfig(
      "AAAAAAAAAAAAAAAAAAA", kDefaultValidity, kDefaultClockDrift));
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code1, timestamp));

  ParentAccessCodeGenerator gen2(ParentAccessCodeConfig(
      "AAAAAAAAAAAAAAAAAAB", kDefaultValidity, kDefaultClockDrift));
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code2, timestamp));

  EXPECT_NE(*code1, *code2);
}

TEST_F(ParentAccessCodeGeneratorTest, DifferentCodeValidity) {
  // Test that codes generated with the different validity are not the same.
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeGenerator gen1(ParentAccessCodeConfig(
      kTestSharedSecret, base::TimeDelta::FromMinutes(1), kDefaultClockDrift));
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code1, timestamp));

  ParentAccessCodeGenerator gen2(ParentAccessCodeConfig(
      kTestSharedSecret, base::TimeDelta::FromMinutes(3), kDefaultClockDrift));
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code2, timestamp));

  EXPECT_NE(*code1, *code2);
}

TEST_F(ParentAccessCodeGeneratorTest, DifferentClockDriftTolerance) {
  // Test that clock drift tolerance does not affect code generation.
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeGenerator gen1(ParentAccessCodeConfig(
      kTestSharedSecret, kDefaultValidity, base::TimeDelta::FromMinutes(1)));
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code1, timestamp));

  ParentAccessCodeGenerator gen2(ParentAccessCodeConfig(
      kTestSharedSecret, kDefaultValidity, base::TimeDelta::FromMinutes(10)));
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Validate(code2, timestamp));

  EXPECT_EQ(*code1, *code2);
}

}  // namespace chromeos
