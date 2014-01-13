// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_calculator.h"

#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefHashCalculatorTest, TestCurrentAlgorithm) {
  base::StringValue string_value_1("string value 1");
  base::StringValue string_value_2("string value 2");
  base::DictionaryValue dictionary_value_1;
  dictionary_value_1.SetInteger("int value", 1);
  dictionary_value_1.Set("nested empty map", new base::DictionaryValue);
  base::DictionaryValue dictionary_value_1_equivalent;
  dictionary_value_1_equivalent.SetInteger("int value", 1);
  base::DictionaryValue dictionary_value_2;
  dictionary_value_2.SetInteger("int value", 2);

  PrefHashCalculator calc1("seed1", "deviceid");
  PrefHashCalculator calc1_dup("seed1", "deviceid");
  PrefHashCalculator calc2("seed2", "deviceid");
  PrefHashCalculator calc3("seed1", "deviceid2");

  // Two calculators with same seed produce same hash.
  ASSERT_EQ(calc1.Calculate("pref_path", &string_value_1),
            calc1_dup.Calculate("pref_path", &string_value_1));
  ASSERT_EQ(PrefHashCalculator::VALID,
            calc1_dup.Validate(
                "pref_path",
                &string_value_1,
                calc1.Calculate("pref_path", &string_value_1)));

  // Different seeds, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc2.Calculate("pref_path", &string_value_1));
  ASSERT_EQ(PrefHashCalculator::INVALID,
            calc2.Validate(
                "pref_path",
                &string_value_1,
                calc1.Calculate("pref_path", &string_value_1)));

  // Different device IDs, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc3.Calculate("pref_path", &string_value_1));

  // Different values, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc1.Calculate("pref_path", &string_value_2));

  // Different paths, different hashes.
  ASSERT_NE(calc1.Calculate("pref_path", &string_value_1),
            calc1.Calculate("pref_path_2", &string_value_1));

  // Works for dictionaries.
  ASSERT_EQ(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_1));
  ASSERT_NE(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_2));

  // Empty dictionary children are pruned.
  ASSERT_EQ(calc1.Calculate("pref_path", &dictionary_value_1),
            calc1.Calculate("pref_path", &dictionary_value_1_equivalent));

  // NULL value is supported.
  ASSERT_FALSE(calc1.Calculate("pref_path", NULL).empty());
}

// Tests the output against a known value to catch unexpected algorithm changes.
TEST(PrefHashCalculatorTest, CatchHashChanges) {
  static const char kSeed[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char kDeviceId[] = "test_device_id1";
  {
    static const char kExpectedValue[] =
        "D8137B8E767D3D910DCD3821CAC61D26ABB042E6EC406AEB0E347ED73A3A4EC1";

    base::ListValue list;
    list.Set(0, new base::FundamentalValue(true));
    list.Set(1, new base::FundamentalValue(100));
    list.Set(2, new base::FundamentalValue(1.0));

    EXPECT_EQ(PrefHashCalculator::VALID,
              PrefHashCalculator(kSeed, kDeviceId).Validate(
                  "pref.path2", &list, kExpectedValue));
  }
  {
    static const char kExpectedValue[] =
        "3F947A044DE9E421A735525385B4C789693682E6F6E3E4CB4741E58724B28F96";

    base::DictionaryValue dict;
    dict.Set("a", new base::StringValue("foo"));
    dict.Set("d", new base::StringValue("bad"));
    dict.Set("b", new base::StringValue("bar"));
    dict.Set("c", new base::StringValue("baz"));

    EXPECT_EQ(PrefHashCalculator::VALID,
              PrefHashCalculator(kSeed, kDeviceId).Validate(
                  "pref.path1", &dict, kExpectedValue));
  }
}

TEST(PrefHashCalculatorTest, TestCompatibilityWithPrefMetricsService) {
  static const char kSeed[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
  };
  static const char kDeviceId[] =
      "D730D9CBD98C734A4FB097A1922275FE9F7E026A4EA1BE0E84";
  static const char kExpectedValue[] =
      "845EF34663FF8D32BE6707F40258FBA531C2BFC532E3B014AFB3476115C2A9DE";

  base::ListValue startup_urls;
  startup_urls.Set(0, new base::StringValue("http://www.chromium.org/"));

  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(std::string(kSeed, arraysize(kSeed)), kDeviceId).
            Validate("session.startup_urls", &startup_urls, kExpectedValue));
}

TEST(PrefHashCalculatorTest, TestLegacyAlgorithm) {
  static const char kExpectedValue[] =
      "C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2";
  static const char kDeviceId[] = "not_used";

  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("foo"));
  dict.Set("d", new base::StringValue("bad"));
  dict.Set("b", new base::StringValue("bar"));
  dict.Set("c", new base::StringValue("baz"));

  // 32 NULL bytes is the seed that was used to generate the legacy hash.
  EXPECT_EQ(PrefHashCalculator::VALID_LEGACY,
            PrefHashCalculator(std::string(32u, 0), kDeviceId).Validate(
                "pref.path1", &dict, kExpectedValue));
}
