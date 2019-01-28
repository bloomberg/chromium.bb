// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_code_authenticator.h"

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

// Configuration that is currently used for PAC.
ParentAccessCodeConfig DefaultConfig() {
  return ParentAccessCodeConfig(kTestSharedSecret, kDefaultValidity,
                                kDefaultClockDrift);
}

// Populates |test_values| with test Parent Access Code data (timestamp - code
// value pairs) generated in Family Link Android app.
void GetTestValues(std::map<base::Time, std::string>* test_values) {
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("8 Jan 2019 16:58:07 PST", &timestamp));
  (*test_values)[timestamp] = "734261";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:35:05 PST", &timestamp));
  (*test_values)[timestamp] = "472150";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:42:49 PST", &timestamp));
  (*test_values)[timestamp] = "204984";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:53:01 PST", &timestamp));
  (*test_values)[timestamp] = "157758";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 16:00:00 PST", &timestamp));
  (*test_values)[timestamp] = "524186";
}

}  // namespace

class ParentAccessCodeAuthenticatorTest : public testing::Test {
 protected:
  ParentAccessCodeAuthenticatorTest() = default;
  ~ParentAccessCodeAuthenticatorTest() override = default;

  // Verifies that |code| is valid for the given |timestamp|.
  void Verify(base::Optional<ParentAccessCode> code, base::Time timestamp) {
    ASSERT_TRUE(code.has_value());
    EXPECT_GE(timestamp, code->valid_from());
    EXPECT_LE(timestamp, code->valid_to());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessCodeAuthenticatorTest);
};

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateHardcodedCodeValues) {
  // Test generation against Parent Access Code values generated in Family
  // Link Android app.
  std::map<base::Time, std::string> test_values;
  ASSERT_NO_FATAL_FAILURE(GetTestValues(&test_values));

  ParentAccessCodeAuthenticator gen(DefaultConfig());
  for (const auto& it : test_values) {
    base::Optional<ParentAccessCode> code = gen.Generate(it.first);
    ASSERT_NO_FATAL_FAILURE(Verify(code, it.first));
    EXPECT_EQ(it.second, code->code());
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateInTheSameTimeBucket) {
  // Test that the same code is generated for whole time bucket defined by code
  // validity period.
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:00:00 PST", &timestamp));
  const ParentAccessCodeConfig config = DefaultConfig();

  ParentAccessCodeAuthenticator gen(config);
  base::Optional<ParentAccessCode> first_code = gen.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(first_code, timestamp));

  int range = (config.code_validity() /
               ParentAccessCodeAuthenticator::kCodeGranularity) -
              1;
  for (int i = 0; i < range; ++i) {
    timestamp += ParentAccessCodeAuthenticator::kCodeGranularity;
    base::Optional<ParentAccessCode> code = gen.Generate(timestamp);
    ASSERT_NO_FATAL_FAILURE(Verify(code, timestamp));
    EXPECT_EQ(*first_code, *code);
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateInDifferentTimeBuckets) {
  // Test that the different codes are generated for different time buckets
  // defined by code validity period.
  base::Time initial_timestamp;
  ASSERT_TRUE(
      base::Time::FromString("14 Jan 2019 15:00:00 PST", &initial_timestamp));

  ParentAccessCodeAuthenticator gen(DefaultConfig());
  base::Optional<ParentAccessCode> first_code = gen.Generate(initial_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(first_code, initial_timestamp));

  for (int i = 1; i < 10; ++i) {
    // "Earlier" time bucket.
    {
      const base::Time timestamp = initial_timestamp - i * kDefaultValidity;
      base::Optional<ParentAccessCode> code = gen.Generate(timestamp);
      ASSERT_NO_FATAL_FAILURE(Verify(code, timestamp));
      EXPECT_NE(*first_code, *code);
    }
    // "Later" time bucket.
    {
      const base::Time timestamp = initial_timestamp + i * kDefaultValidity;
      base::Optional<ParentAccessCode> code = gen.Generate(timestamp);
      ASSERT_NO_FATAL_FAILURE(Verify(code, timestamp));
      EXPECT_NE(*first_code, *code);
    }
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateWithSameTimestamp) {
  // Test that codes generated with the same timestamp and config are the same.
  const ParentAccessCodeConfig config = DefaultConfig();
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeAuthenticator gen1(config);
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  ParentAccessCodeAuthenticator gen2(config);
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_EQ(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateWithDifferentSharedSecret) {
  // Test that codes generated with the different secrets are not the same.
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeAuthenticator gen1(ParentAccessCodeConfig(
      "AAAAAAAAAAAAAAAAAAA", kDefaultValidity, kDefaultClockDrift));
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  ParentAccessCodeAuthenticator gen2(ParentAccessCodeConfig(
      "AAAAAAAAAAAAAAAAAAB", kDefaultValidity, kDefaultClockDrift));
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_NE(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateWithDifferentCodeValidity) {
  // Test that codes generated with the different validity are not the same.
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeAuthenticator gen1(ParentAccessCodeConfig(
      kTestSharedSecret, base::TimeDelta::FromMinutes(1), kDefaultClockDrift));
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  ParentAccessCodeAuthenticator gen2(ParentAccessCodeConfig(
      kTestSharedSecret, base::TimeDelta::FromMinutes(3), kDefaultClockDrift));
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_NE(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest,
       GenerateWihtDifferentClockDriftTolerance) {
  // Test that clock drift tolerance does not affect code generation.
  const base::Time timestamp = base::Time::Now();

  ParentAccessCodeAuthenticator gen1(ParentAccessCodeConfig(
      kTestSharedSecret, kDefaultValidity, base::TimeDelta::FromMinutes(1)));
  base::Optional<ParentAccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  ParentAccessCodeAuthenticator gen2(ParentAccessCodeConfig(
      kTestSharedSecret, kDefaultValidity, base::TimeDelta::FromMinutes(10)));
  base::Optional<ParentAccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_EQ(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest, ValidateHardcodedCodeValues) {
  // Test validation against Parent Access Code values generated in Family Link
  // Android app.
  std::map<base::Time, std::string> test_values;
  ASSERT_NO_FATAL_FAILURE(GetTestValues(&test_values));

  ParentAccessCodeAuthenticator gen(ParentAccessCodeConfig(
      kTestSharedSecret, kDefaultValidity, base::TimeDelta::FromMinutes(0)));
  for (const auto& it : test_values) {
    base::Optional<ParentAccessCode> code = gen.Validate(it.second, it.first);
    ASSERT_NO_FATAL_FAILURE(Verify(code, it.first));
    EXPECT_EQ(it.second, code->code());
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest,
       ValidationAndGenerationOnDifferentAuthenticators) {
  // Test validation against codes generated by separate
  // ParentAccessCodeAuthenticator object in and outside of the valid time
  // bucket.
  const ParentAccessCodeConfig config(kTestSharedSecret, kDefaultValidity,
                                      base::TimeDelta::FromMinutes(0));
  ParentAccessCodeAuthenticator generator(config);
  ParentAccessCodeAuthenticator validator(config);

  base::Time generation_timestamp;
  ASSERT_TRUE(base::Time::FromString("15 Jan 2019 00:00:00 PST",
                                     &generation_timestamp));

  base::Optional<ParentAccessCode> generated_code =
      generator.Generate(generation_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(generated_code, generation_timestamp));

  // Before valid period.
  base::Optional<ParentAccessCode> validated_code = validator.Validate(
      generated_code->code(),
      generation_timestamp - base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(validated_code);

  // In valid period.
  int range =
      config.code_validity() / ParentAccessCodeAuthenticator::kCodeGranularity;
  for (int i = 0; i < range; ++i) {
    validated_code = validator.Validate(
        generated_code->code(),
        generation_timestamp +
            i * ParentAccessCodeAuthenticator::kCodeGranularity);
    ASSERT_TRUE(validated_code);
    EXPECT_EQ(*generated_code, *validated_code);
  }

  // After valid period.
  validated_code = validator.Validate(
      generated_code->code(), generation_timestamp + config.code_validity());
  EXPECT_FALSE(validated_code);
}

TEST_F(ParentAccessCodeAuthenticatorTest,
       ValidationAndGenerationOnSameAuthenticator) {
  // Test generation and validation on the same ParentAccessCodeAuthenticator
  // object in and outside of the valid time bucket.
  const ParentAccessCodeConfig config(kTestSharedSecret, kDefaultValidity,
                                      base::TimeDelta::FromMinutes(0));
  ParentAccessCodeAuthenticator authenticator(config);

  base::Time generation_timestamp;
  ASSERT_TRUE(base::Time::FromString("15 Jan 2019 00:00:00 PST",
                                     &generation_timestamp));

  base::Optional<ParentAccessCode> generated_code =
      authenticator.Generate(generation_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(generated_code, generation_timestamp));

  // Before valid period.
  base::Optional<ParentAccessCode> validated_code = authenticator.Validate(
      generated_code->code(),
      generation_timestamp - base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(validated_code);

  // In valid period.
  int range =
      config.code_validity() / ParentAccessCodeAuthenticator::kCodeGranularity;
  for (int i = 0; i < range; ++i) {
    validated_code = authenticator.Validate(
        generated_code->code(),
        generation_timestamp +
            i * ParentAccessCodeAuthenticator::kCodeGranularity);
    ASSERT_TRUE(validated_code);
    EXPECT_EQ(*generated_code, *validated_code);
  }

  // After valid period.
  validated_code = authenticator.Validate(
      generated_code->code(), generation_timestamp + config.code_validity());
  EXPECT_FALSE(validated_code);
}

TEST_F(ParentAccessCodeAuthenticatorTest, ValidationWithClockDriftTolerance) {
  // Test validation with clock drift tolerance.
  ParentAccessCodeAuthenticator generator(DefaultConfig());
  ParentAccessCodeAuthenticator validator_with_tolerance(DefaultConfig());
  ParentAccessCodeAuthenticator validator_no_tolerance(ParentAccessCodeConfig(
      kTestSharedSecret, kDefaultValidity, base::TimeDelta::FromMinutes(0)));

  // By default code will be valid [15:30:00-15:40:00).
  // With clock drift tolerance code will be valid [15:25:00-15:45:00).
  base::Time generation_timestamp;
  ASSERT_TRUE(base::Time::FromString("15 Jan 2019 15:30:00 PST",
                                     &generation_timestamp));

  base::Optional<ParentAccessCode> generated_code =
      generator.Generate(generation_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(generated_code, generation_timestamp));

  // Both validators accept the code in valid period.
  int range =
      kDefaultValidity / ParentAccessCodeAuthenticator::kCodeGranularity;
  base::Time timestamp;
  base::Optional<ParentAccessCode> validated_code_no_tolerance;
  base::Optional<ParentAccessCode> validated_code_with_tolerance;
  for (int i = 0; i < range; ++i) {
    timestamp = generation_timestamp +
                i * ParentAccessCodeAuthenticator::kCodeGranularity;

    validated_code_no_tolerance =
        validator_no_tolerance.Validate(generated_code->code(), timestamp);
    ASSERT_TRUE(validated_code_no_tolerance);

    validated_code_with_tolerance =
        validator_with_tolerance.Validate(generated_code->code(), timestamp);
    ASSERT_TRUE(validated_code_with_tolerance);

    EXPECT_EQ(*validated_code_no_tolerance, *validated_code_with_tolerance);
  }

  // Validator's device clock late by tolerated drift.
  timestamp = generation_timestamp - kDefaultClockDrift / 2;
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_TRUE(validated_code_with_tolerance);

  // Validator's device clock late outside of tolerated drift.
  timestamp = generation_timestamp - kDefaultClockDrift -
              base::TimeDelta::FromSeconds(1);
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_with_tolerance);

  // Validator's device clock ahead by tolerated drift.
  timestamp = generation_timestamp + kDefaultValidity + kDefaultClockDrift / 2;
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_TRUE(validated_code_with_tolerance);

  // Validator's device clock ahead outside of tolerated drift.
  timestamp = generation_timestamp + kDefaultValidity + kDefaultClockDrift;
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_with_tolerance);
}

}  // namespace chromeos
