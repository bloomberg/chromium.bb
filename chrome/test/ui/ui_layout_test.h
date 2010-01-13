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
                               int port);
  void AddResourceForLayoutTest(const FilePath& parent_dir,
                                const FilePath& resource_dir);
  void RunLayoutTest(const std::string& test_case_file_name,
                     int port);

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
  static const int kNoHttpPort = -1;
  static const int kHttpPort = 8080;
  static const int kWebSocketPort = 8880;
};

#endif  // CHROME_TEST_UI_UI_LAYOUT_TEST_H_
