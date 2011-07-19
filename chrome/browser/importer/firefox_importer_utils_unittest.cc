// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/importer/firefox_importer_utils.h"

struct GetPrefsJsValueCase {
  std::string prefs_content;
  std::string pref_name;
  std::string pref_value;
} GetPrefsJsValueCases[] = {
  // Basic case. Single pref, unquoted value.
  { "user_pref(\"foo.bar\", 1);", "foo.bar", "1" },
  // Value is quoted. Quotes should be stripped.
  { "user_pref(\"foo.bar\", \"1\");", "foo.bar", "1" },
  // Value has parens.
  { "user_pref(\"foo.bar\", \"Value (detail)\");",
    "foo.bar", "Value (detail)" },
  // Multi-line case.
  { "user_pref(\"foo.bar\", 1);\n"
    "user_pref(\"foo.baz\", 2);\n"
    "user_pref(\"foo.bag\", 3);",
    "foo.baz", "2" },
  // Malformed content.
  { "user_pref(\"foo.bar\", 1);\n"
    "user_pref(\"foo.baz\", 2;\n"
    "user_pref(\"foo.bag\", 3);",
    "foo.baz", "" },
  // Malformed content.
  { "uesr_pref(\"foo.bar\", 1);", "foo.bar", "" },
};

TEST(FirefoxImporterUtilsTest, GetPrefsJsValue) {
  for (size_t i = 0; i < arraysize(GetPrefsJsValueCases); ++i) {
    EXPECT_EQ(
      GetPrefsJsValueCases[i].pref_value,
      GetPrefsJsValue(GetPrefsJsValueCases[i].prefs_content,
                      GetPrefsJsValueCases[i].pref_name));
  }
}
