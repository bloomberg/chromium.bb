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
  const char* kDeviceId = "test_device_id1";
  {
    static const char kExpectedValue[] =
        "5CE37D7EBCBC9BE510F0F5E7C326CA92C1673713C3717839610AEA1A217D8BB8";

    base::ListValue list;
    list.Set(0, new base::FundamentalValue(true));
    list.Set(1, new base::FundamentalValue(100));
    list.Set(2, new base::FundamentalValue(1.0));

    // 32 NULL bytes is the seed that was used to generate the hash in old
    // tests. Use it again to ensure that we haven't altered the algorithm.
    EXPECT_EQ(PrefHashCalculator::VALID,
              PrefHashCalculator(std::string(32u, 0), kDeviceId).Validate(
                  "pref.path2", &list, kExpectedValue));
  }
  {
    static const char kExpectedValue[] =
        "A50FE7EB31BFBC32B8A27E71730AF15421178A9B5815644ACE174B18966735B9";

    base::DictionaryValue dict;
    dict.Set("a", new base::StringValue("foo"));
    dict.Set("d", new base::StringValue("bad"));
    dict.Set("b", new base::StringValue("bar"));
    dict.Set("c", new base::StringValue("baz"));

    // 32 NULL bytes is the seed that was used to generate the hash in old
    // tests. Use it again to ensure that we haven't altered the algorithm.
    EXPECT_EQ(PrefHashCalculator::VALID,
              PrefHashCalculator(std::string(32u, 0), kDeviceId).Validate(
                  "pref.path1", &dict, kExpectedValue));
  }
}

TEST(PrefHashCalculatorTest, TestLegacyAlgorithm) {
  const char* kExpectedValue =
      "C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2";
  const char* kDeviceId = "deviceid";

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
