// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

std::vector<SplitTestName> GetCompiledInTests() {
  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  std::vector<SplitTestName> tests;
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      tests.push_back(std::make_pair(test_case->name(), test_info->name()));
    }
  }
  return tests;
}

bool WriteCompiledInTestsToFile(const FilePath& path) {
  std::vector<SplitTestName> tests(GetCompiledInTests());

  ListValue root;
  for (size_t i = 0; i < tests.size(); ++i) {
    DictionaryValue* test_info = new DictionaryValue;
    test_info->SetString("test_case_name", tests[i].first);
    test_info->SetString("test_name", tests[i].second);
    root.Append(test_info);
  }

  JSONFileValueSerializer serializer(path);
  return serializer.Serialize(root);
}

}  // namespace
