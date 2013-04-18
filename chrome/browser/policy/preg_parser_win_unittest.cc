// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/preg_parser_win.h"

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_load_status.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace preg_parser {

TEST(PRegParserWinTest, TestParseFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));

  // Prepare the test dictionary with some data so the test can check that the
  // PReg action triggers work, i.e. remove these items.
  base::DictionaryValue dict;
  dict.SetInteger("DeleteValuesTest1", 1);
  dict.SetString("DeleteValuesTest2", "2");
  dict.SetInteger("DeleteKeysTest1", 1);
  dict.SetString("DeleteKeysTest2", "2");
  dict.SetInteger("DelTest", 1);
  base::DictionaryValue* subdict = new base::DictionaryValue();
  subdict->SetInteger("DelValsTest1", 1);
  subdict->SetString("DelValsTest2", "2");
  subdict->Set("DelValsTest3", new base::DictionaryValue());
  dict.Set("DelValsTest", subdict);

  // Run the parser.
  base::FilePath test_file(test_data_dir.AppendASCII("policy/registry.pol"));
  PolicyLoadStatusSample status;
  ASSERT_TRUE(preg_parser::ReadFile(
      test_file, L"SOFTWARE\\Policies\\Chromium", &dict, &status));

  // Build the expected output dictionary.
  base::DictionaryValue expected;
  base::DictionaryValue* del_vals_dict = new base::DictionaryValue();
  del_vals_dict->Set("DelValsTest3", new base::DictionaryValue());
  expected.Set("DelValsTest", del_vals_dict);
  expected.SetInteger("HomepageIsNewTabPage", 1);
  expected.SetString("HomepageLocation", "http://www.example.com");
  expected.SetInteger("RestoreOnStartup", 4);
  base::DictionaryValue* startup_urls = new DictionaryValue();
  startup_urls->SetString("1", "http://www.chromium.org");
  startup_urls->SetString("2", "http://www.example.com");
  expected.Set("RestoreOnStartupURLs", startup_urls);
  expected.SetInteger("ShowHomeButton", 1);
  expected.SetString("Snowman", "\xE2\x98\x83");

  EXPECT_TRUE(base::Value::Equals(&expected, &dict));
}

}  // namespace policy
}  // namespace preg_parser
