// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/apple_event_util.h"

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest_mac.h"

namespace {

class AppleEventUtilTest : public CocoaTest { };

struct TestCase {
  const char* json_input;
  const char* expected_aedesc_dump;
  DescType expected_aedesc_type;
};

TEST_F(AppleEventUtilTest, DISABLED_ValueToAppleEventDescriptor) {
  const struct TestCase cases[] = {
    { "null",         "'msng'",             typeType },
    { "-1000",        "-1000",              typeSInt32 },
    { "0",            "0",                  typeSInt32 },
    { "1000",         "1000",               typeSInt32 },
    { "-1e100",       "-1e+100",            typeIEEE64BitFloatingPoint },
    { "0.0",          "0",                  typeIEEE64BitFloatingPoint },
    { "1e100",        "1e+100",             typeIEEE64BitFloatingPoint },
    { "\"\"",         "'utxt'(\"\")",       typeUnicodeText },
    { "\"string\"",   "'utxt'(\"string\")", typeUnicodeText },
    { "{}",           "{ 'usrf':[  ] }",    typeAERecord },
    { "[]",           "[  ]",               typeAEList },
    { "{\"Image\": {"
      "\"Width\": 800,"
      "\"Height\": 600,"
      "\"Title\": \"View from 15th Floor\","
      "\"Thumbnail\": {"
      "\"Url\": \"http://www.example.com/image/481989943\","
      "\"Height\": 125,"
      "\"Width\": \"100\""
      "},"
      "\"IDs\": [116, 943, 234, 38793]"
      "}"
      "}",
      "{ 'usrf':[ 'utxt'(\"Image\"), { 'usrf':[ 'utxt'(\"Height\"), 600, "
      "'utxt'(\"IDs\"), [ 116, 943, 234, 38793 ], 'utxt'(\"Thumbnail\"), "
      "{ 'usrf':[ 'utxt'(\"Height\"), 125, 'utxt'(\"Url\"), "
      "'utxt'(\"http://www.example.com/image/481989943\"), 'utxt'(\"Width\"), "
      "'utxt'(\"100\") ] }, 'utxt'(\"Title\"), "
      "'utxt'(\"View from 15th Floor\"), 'utxt'(\"Width\"), 800 ] } ] }",
      typeAERecord },
    { "["
      "{"
      "\"precision\": \"zip\","
      "\"Latitude\": 37.7668,"
      "\"Longitude\": -122.3959,"
      "\"Address\": \"\","
      "\"City\": \"SAN FRANCISCO\","
      "\"State\": \"CA\","
      "\"Zip\": \"94107\","
      "\"Country\": \"US\""
      "},"
      "{"
      "\"precision\": \"zip\","
      "\"Latitude\": 37.371991,"
      "\"Longitude\": -122.026020,"
      "\"Address\": \"\","
      "\"City\": \"SUNNYVALE\","
      "\"State\": \"CA\","
      "\"Zip\": \"94085\","
      "\"Country\": \"US\""
      "}"
      "]",
      "[ { 'usrf':[ 'utxt'(\"Address\"), 'utxt'(\"\"), 'utxt'(\"City\"), "
      "'utxt'(\"SAN FRANCISCO\"), 'utxt'(\"Country\"), 'utxt'(\"US\"), "
      "'utxt'(\"Latitude\"), 37.7668, 'utxt'(\"Longitude\"), -122.396, "
      "'utxt'(\"State\"), 'utxt'(\"CA\"), 'utxt'(\"Zip\"), 'utxt'(\"94107\"), "
      "'utxt'(\"precision\"), 'utxt'(\"zip\") ] }, { 'usrf':[ "
      "'utxt'(\"Address\"), 'utxt'(\"\"), 'utxt'(\"City\"), "
      "'utxt'(\"SUNNYVALE\"), 'utxt'(\"Country\"), 'utxt'(\"US\"), "
      "'utxt'(\"Latitude\"), 37.372, 'utxt'(\"Longitude\"), -122.026, "
      "'utxt'(\"State\"), 'utxt'(\"CA\"), 'utxt'(\"Zip\"), 'utxt'(\"94085\"), "
      "'utxt'(\"precision\"), 'utxt'(\"zip\") ] } ]",
      typeAEList },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(cases[i].json_input));
    NSAppleEventDescriptor* descriptor =
        chrome::mac::ValueToAppleEventDescriptor(value.get());
    NSString* descriptor_description = [descriptor description];

    std::string expected_contents =
        base::StringPrintf("<NSAppleEventDescriptor: %s>",
                           cases[i].expected_aedesc_dump);
    EXPECT_STREQ(expected_contents.c_str(),
                 [descriptor_description UTF8String]) << "i: " << i;
    EXPECT_EQ(cases[i].expected_aedesc_type,
              [descriptor descriptorType]) << "i: " << i;
  }

  // Test boolean values separately because boolean NSAppleEventDescriptors
  // return different values across different system versions when their
  // -description method is called.

  const bool all_bools[] = { true, false };
  for (bool b : all_bools) {
    base::FundamentalValue value(b);
    NSAppleEventDescriptor* descriptor =
        chrome::mac::ValueToAppleEventDescriptor(&value);

    EXPECT_EQ(typeBoolean, [descriptor descriptorType]);
    EXPECT_EQ(b, [descriptor booleanValue]);
  }
}

}  // namespace
