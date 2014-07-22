// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_calculator.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/tracked/pref_hash_calculator_helper.h"
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
// The test hashes below must NEVER be updated, the serialization algorithm used
// must always be able to generate data that will produce these exact hashes.
TEST(PrefHashCalculatorTest, CatchHashChanges) {
  static const char kSeed[] = "0123456789ABCDEF0123456789ABCDEF";
  static const char kDeviceId[] = "test_device_id1";

  scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
  scoped_ptr<base::Value> bool_value(new base::FundamentalValue(false));
  scoped_ptr<base::Value> int_value(new base::FundamentalValue(1234567890));
  scoped_ptr<base::Value> double_value(
      new base::FundamentalValue(123.0987654321));
  scoped_ptr<base::Value> string_value(
      new base::StringValue("testing with special chars:\n<>{}:^^@#$\\/"));

  // For legacy reasons, we have to support pruning of empty lists/dictionaries
  // and nested empty ists/dicts in the hash generation algorithm.
  scoped_ptr<base::DictionaryValue> nested_empty_dict(
      new base::DictionaryValue);
  nested_empty_dict->Set("a", new base::DictionaryValue);
  nested_empty_dict->Set("b", new base::ListValue);
  scoped_ptr<base::ListValue> nested_empty_list(
      new base::ListValue);
  nested_empty_list->Append(new base::DictionaryValue);
  nested_empty_list->Append(new base::ListValue);
  nested_empty_list->Append(nested_empty_dict->DeepCopy());

  // A dictionary with an empty dictionary, an empty list, and nested empty
  // dictionaries/lists in it.
  scoped_ptr<base::DictionaryValue> dict_value(new base::DictionaryValue);
  dict_value->Set("a", new base::StringValue("foo"));
  dict_value->Set("d", new base::ListValue);
  dict_value->Set("b", new base::DictionaryValue);
  dict_value->Set("c", new base::StringValue("baz"));
  dict_value->Set("e", nested_empty_dict.release());
  dict_value->Set("f", nested_empty_list.release());

  scoped_ptr<base::ListValue> list_value(new base::ListValue);
  list_value->AppendBoolean(true);
  list_value->AppendInteger(100);
  list_value->AppendDouble(1.0);

  ASSERT_EQ(base::Value::TYPE_NULL, null_value->GetType());
  ASSERT_EQ(base::Value::TYPE_BOOLEAN, bool_value->GetType());
  ASSERT_EQ(base::Value::TYPE_INTEGER, int_value->GetType());
  ASSERT_EQ(base::Value::TYPE_DOUBLE, double_value->GetType());
  ASSERT_EQ(base::Value::TYPE_STRING, string_value->GetType());
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, dict_value->GetType());
  ASSERT_EQ(base::Value::TYPE_LIST, list_value->GetType());

  // Test every value type independently. Intentionally omits TYPE_BINARY which
  // isn't even allowed in JSONWriter's input.
  static const char kExpectedNullValue[] =
      "C2871D0AC76176E39948C50A9A562B863E610FDA90C675A6A8AD16B4DC4F53DC";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", null_value.get(), kExpectedNullValue));

  static const char kExpectedBooleanValue[] =
      "A326E2F405CFE05D08289CDADD9DB4F529592F0945A8CE204289E4C930D8AA43";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", bool_value.get(), kExpectedBooleanValue));

  static const char kExpectedIntegerValue[] =
      "4B69938F802A2A26D69467F3E1E4A474F6323C64EFC54DBDB4A5708A7D005042";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", int_value.get(), kExpectedIntegerValue));

  static const char kExpectedDoubleValue[] =
      "1734C9C745B9C92D896B9A710994BF1B56D55BFB0F00C207EC995152AF02F08F";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", double_value.get(), kExpectedDoubleValue));

  static const char kExpectedStringValue[] =
      "154D15522C856AA944BFA5A9E3FFB46925BF2B95A10199564651CA1C13E98433";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", string_value.get(), kExpectedStringValue));

  static const char kExpectedDictValue[] =
      "597CECCBF930AF1FFABAC6AF3851C062867C134B4D5A06BDB3B03B988A182CBB";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", dict_value.get(), kExpectedDictValue));

  static const char kExpectedListValue[] =
      "4E2CC0A9B8DF8C5049C53E8B139007792EC6295239545BC99BBF9CDE8A2F5E30";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", list_value.get(), kExpectedListValue));

  // Also test every value type together in the same dictionary.
  base::DictionaryValue everything;
  everything.Set("null", null_value.release());
  everything.Set("bool", bool_value.release());
  everything.Set("int", int_value.release());
  everything.Set("double", double_value.release());
  everything.Set("string", string_value.release());
  everything.Set("list", list_value.release());
  everything.Set("dict", dict_value.release());
  static const char kExpectedEverythingValue[] =
      "5A9D15E4D2FA909007EDE6A18605735E3EB712E2EDE83D6735CE5DD96A5AFBAA";
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kDeviceId).Validate(
                "pref.path", &everything, kExpectedEverythingValue));
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

TEST(PrefHashCalculatorTest, TestLegacyNoDeviceIdNoPathAlgorithm) {
  static const char kTestedLegacyHash[] =
      "C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2";
  static const char kDeviceId[] = "not_used";

  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("foo"));
  dict.Set("d", new base::StringValue("bad"));
  dict.Set("b", new base::StringValue("bar"));
  dict.Set("c", new base::StringValue("baz"));

  // 32 NULL bytes is the seed that was used to generate the legacy hash.
  EXPECT_EQ(PrefHashCalculator::VALID_WEAK_LEGACY,
            PrefHashCalculator(std::string(32u, 0), kDeviceId).Validate(
                "unused_path", &dict, kTestedLegacyHash));
}

std::string MockGetLegacyDeviceId(const std::string& modern_device_id) {
  if (modern_device_id.empty())
    return std::string();
  return modern_device_id + "_LEGACY";
}

TEST(PrefHashCalculatorTest, TestLegacyDeviceIdAlgorithm) {
  // The full algorithm should kick in when the device id is non-empty and we
  // should thus get VALID_SECURE_LEGACY on verification.
  static const char kDeviceId[] = "DEVICE_ID";
  static const char kSeed[] = "01234567890123456789012345678901";
  static const char kPrefPath[] = "test.pref";
  static const char kPrefValue[] = "http://example.com/";
  // Test hash based on the mock legacy id (based on kDeviceId) + kPrefPath +
  // kPrefValue under kSeed.
  static const char kTestedHash[] =
      "09ABD84B13E4366B24DFF898C8C4614E033514B4E2EF3C6810F50B63273C83AD";

  const base::StringValue string_value(kPrefValue);
  EXPECT_EQ(PrefHashCalculator::VALID_SECURE_LEGACY,
            PrefHashCalculator(kSeed, kDeviceId,
                               base::Bind(&MockGetLegacyDeviceId)).Validate(
                kPrefPath, &string_value, kTestedHash));
}

TEST(PrefHashCalculatorTest, TestLegacyDeviceIdAlgorithmOnEmptyDeviceId) {
  // MockGetLegacyDeviceId will return a legacy device ID that is the same
  // (empty) as the modern device ID here. So this MAC will be valid using
  // either ID. The PrefHashCalculator should return VALID, not
  // VALID_SECURE_LEGACY, in this case.
  static const char kEmptyDeviceId[] = "";
  static const char kSeed[] = "01234567890123456789012345678901";
  static const char kPrefPath[] = "test.pref";
  static const char kPrefValue[] = "http://example.com/";
  // Test hash based on an empty legacy device id + kPrefPath + kPrefValue under
  // kSeed.
  static const char kTestedHash[] =
      "842C71283B9C3D86AA934CD639FDB0428BF0E2B6EC8537A21575CC4C4FA0A615";

  const base::StringValue string_value(kPrefValue);
  EXPECT_EQ(PrefHashCalculator::VALID,
            PrefHashCalculator(kSeed, kEmptyDeviceId,
                               base::Bind(&MockGetLegacyDeviceId)).Validate(
                kPrefPath, &string_value, kTestedHash));
}
