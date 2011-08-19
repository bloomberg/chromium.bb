// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History UI tests

#include "chrome/browser/history/history_uitest.h"

namespace {

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const FilePath::CharType* const kHistoryDir = FILE_PATH_LITERAL("History");

}  // namespace

TEST_F(HistoryTester, VerifyHistoryLength1) {
  // Test the history length for the following page transitions.
  //   -open-> Page 1.

  const FilePath test_case_1(
      FILE_PATH_LITERAL("history_length_test_page_1.html"));
  GURL url_1 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_1);
  NavigateToURL(url_1);
  WaitForFinish("History_Length_Test_1", "1", url_1, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(HistoryTester, VerifyHistoryLength2) {
  // Test the history length for the following page transitions.
  //   -open-> Page 2 -redirect-> Page 3.

  const FilePath test_case_2(
      FILE_PATH_LITERAL("history_length_test_page_2.html"));
  GURL url_2 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_2);
  NavigateToURL(url_2);
  WaitForFinish("History_Length_Test_2", "1", url_2, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(HistoryTester, VerifyHistoryLength3) {
  // Test the history length for the following page transitions.
  // -open-> Page 1 -> open Page 2 -redirect Page 3. open Page 4
  // -navigate_backward-> Page 3 -navigate_backward->Page 1
  // -navigate_forward-> Page 3 -navigate_forward-> Page 4
  const FilePath test_case_1(
      FILE_PATH_LITERAL("history_length_test_page_1.html"));
  GURL url_1 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_1);
  NavigateToURL(url_1);
  WaitForFinish("History_Length_Test_1", "1", url_1, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());

  const FilePath test_case_2(
      FILE_PATH_LITERAL("history_length_test_page_2.html"));
  GURL url_2 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_2);
  NavigateToURL(url_2);
  WaitForFinish("History_Length_Test_2", "1", url_2, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());

  const FilePath test_case_3(
      FILE_PATH_LITERAL("history_length_test_page_4.html"));
  GURL url_3 = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case_3);
  NavigateToURL(url_3);
  WaitForFinish("History_Length_Test_3", "1", url_3, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}

TEST_F(HistoryTester, ConsiderSlowRedirectAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 21 -redirect-> Page 22.
  //
  // If redirect occurs more than 5 seconds later after the page is loaded,
  // the redirect is likely to be user-initiated.
  // Therefore, Page 21 should be in the history in addition to Page 22.

  const FilePath test_case(
      FILE_PATH_LITERAL("history_length_test_page_21.html"));
  GURL url = ui_test_utils::GetTestUrl(FilePath(kHistoryDir), test_case);
  NavigateToURL(url);
  WaitForFinish("History_Length_Test_21", "1", url, kTestCompleteCookie,
                kTestCompleteSuccess, TestTimeouts::action_max_timeout_ms());
}
