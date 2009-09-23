// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_LAYOUT_TEST_H_
#define CHROME_TEST_UI_UI_LAYOUT_TEST_H_

#include "base/file_path.h"
#include "chrome/test/ui/ui_test.h"

class UILayoutTest : public UITest {
 protected:
  UILayoutTest();
  virtual ~UILayoutTest();

  void InitializeForLayoutTest(const FilePath& test_parent_dir,
                               const FilePath& test_case_dir,
                               bool is_http_test);
  void RunLayoutTest(const std::string& test_case_file_name,
                     bool is_http_test);

  bool ReadExpectedResult(const FilePath& result_dir_path,
                          const std::string test_case_file_name,
                          std::string* expected_result_value);

  bool initialized_for_layout_test_;
  int test_count_;
  FilePath temp_test_dir_;
  FilePath layout_test_dir_;
  FilePath test_case_dir_;
  FilePath new_http_root_dir_;
  FilePath new_layout_test_dir_;
  FilePath rebase_result_dir_;
  FilePath rebase_result_win_dir_;
  std::string layout_test_controller_;

  static const int kTestIntervalMs = 250;
  static const int kTestWaitTimeoutMs = 60 * 1000;
};

#endif  // CHROME_TEST_UI_UI_LAYOUT_TEST_H_
