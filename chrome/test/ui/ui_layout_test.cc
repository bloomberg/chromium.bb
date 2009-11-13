// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_layout_test.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
static const char kPlatformName[] = "chromium-win";
#elif defined(OS_MACOSX)
static const char kPlatformName[] = "chromium-mac";
#elif defined(OS_LINUX)
static const char kPlatformName[] = "chromium-linux";
#else
#error No known OS defined
#endif

static const char kTestCompleteCookie[] = "status";

UILayoutTest::UILayoutTest()
    : initialized_for_layout_test_(false),
      test_count_(0) {
}

UILayoutTest::~UILayoutTest() {
  if (!temp_test_dir_.empty()) {
    // The deletion might fail because HTTP server process might not been
    // completely shut down yet and is still holding certain handle to it.
    // To work around this problem, we try to repeat the deletion several
    // times.
    EXPECT_TRUE(file_util::DieFileDie(temp_test_dir_, true));
  }
}

void UILayoutTest::InitializeForLayoutTest(const FilePath& test_parent_dir,
                                           const FilePath& test_case_dir,
                                           bool is_http_test) {
  FilePath src_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);

  // Gets the file path to WebKit ui layout tests, that is,
  //   chrome/test/data/ui_tests/LayoutTests/...
  // Note that we have to use our own copy of WebKit layout tests because our
  // build machines do not have WebKit layout tests added.
  layout_test_dir_ = src_dir.AppendASCII("chrome");
  layout_test_dir_ = layout_test_dir_.AppendASCII("test");
  layout_test_dir_ = layout_test_dir_.AppendASCII("data");
  layout_test_dir_ = layout_test_dir_.AppendASCII("layout_tests");
  layout_test_dir_ = layout_test_dir_.Append(test_parent_dir);
  layout_test_dir_ = layout_test_dir_.Append(test_case_dir);
  ASSERT_TRUE(file_util::DirectoryExists(layout_test_dir_));

  // Gets the file path to rebased expected result directory for the current
  // platform.
  //   webkit/data/layout_tests/platform/chromium_***/LayoutTests/...
  rebase_result_dir_ = src_dir.AppendASCII("webkit");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII("data");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII("layout_tests");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII("platform");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII(kPlatformName);
  rebase_result_dir_ = rebase_result_dir_.Append(test_parent_dir);
  rebase_result_dir_ = rebase_result_dir_.Append(test_case_dir);

  // Gets the file path to rebased expected result directory under the
  // win32 platform. This is used by other non-win32 platform to use the same
  // rebased expected results.
#if !defined(OS_WIN)
  rebase_result_win_dir_ = src_dir.AppendASCII("webkit");
  rebase_result_win_dir_ = rebase_result_win_dir_.AppendASCII("data");
  rebase_result_win_dir_ = rebase_result_win_dir_.AppendASCII("layout_tests");
  rebase_result_win_dir_ = rebase_result_win_dir_.AppendASCII("platform");
  rebase_result_win_dir_ = rebase_result_win_dir_.AppendASCII("chromium-win");
  rebase_result_win_dir_ = rebase_result_win_dir_.Append(test_parent_dir);
  rebase_result_win_dir_ = rebase_result_win_dir_.Append(test_case_dir);
#endif

  // Creates the temporary directory.
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_ui_layout_tests_"), &temp_test_dir_));

  // Creates the new layout test subdirectory under the temp directory.
  // Note that we have to mimic the same layout test directory structure,
  // like .../LayoutTests/fast/workers/.... Otherwise those layout tests
  // dealing with location property, like worker-location.html, could fail.
  new_layout_test_dir_ = temp_test_dir_;
  new_layout_test_dir_ = new_layout_test_dir_.Append(test_parent_dir);
  if (is_http_test) {
    new_http_root_dir_ = new_layout_test_dir_;
    test_case_dir_ = test_case_dir;
  }
  new_layout_test_dir_ = new_layout_test_dir_.Append(test_case_dir);
  ASSERT_TRUE(file_util::CreateDirectory(new_layout_test_dir_));

  // Copy the resource subdirectory if it exists.
  FilePath layout_test_resource_path(layout_test_dir_);
  layout_test_resource_path =
      layout_test_resource_path.AppendASCII("resources");
  FilePath new_layout_test_resource_path(new_layout_test_dir_);
  new_layout_test_resource_path =
      new_layout_test_resource_path.AppendASCII("resources");
  file_util::CopyDirectory(layout_test_resource_path,
                           new_layout_test_resource_path, true);

  // Copies the parent resource subdirectory. This is needed in order to run
  // http layout tests.
  if (is_http_test) {
    FilePath parent_resource_path(layout_test_dir_.DirName());
    parent_resource_path = parent_resource_path.AppendASCII("resources");
    FilePath new_parent_resource_path(new_layout_test_dir_.DirName());
    new_parent_resource_path =
        new_parent_resource_path.AppendASCII("resources");
    ASSERT_TRUE(file_util::CopyDirectory(
        parent_resource_path, new_parent_resource_path, true));
  }

  // Reads the layout test controller simulation script.
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("layout_tests");
  path = path.AppendASCII("layout_test_controller.html");
  ASSERT_TRUE(file_util::ReadFileToString(path, &layout_test_controller_));
}

void UILayoutTest::AddResourceForLayoutTest(const FilePath& parent_dir,
                                            const FilePath& resource_dir) {
  FilePath root_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &root_dir);

  FilePath src_dir = root_dir.AppendASCII("chrome");
  src_dir = src_dir.AppendASCII("test");
  src_dir = src_dir.AppendASCII("data");
  src_dir = src_dir.AppendASCII("layout_tests");
  src_dir = src_dir.Append(parent_dir);
  src_dir = src_dir.Append(resource_dir);
  ASSERT_TRUE(file_util::DirectoryExists(src_dir));

  FilePath dest_parent_dir = temp_test_dir_.Append(parent_dir);
  ASSERT_TRUE(file_util::CreateDirectory(dest_parent_dir));
  FilePath dest_dir = dest_parent_dir.Append(resource_dir);
  ASSERT_TRUE(file_util::CopyDirectory(src_dir, dest_dir, true));
}

void UILayoutTest::RunLayoutTest(const std::string& test_case_file_name,
                                 bool is_http_test) {
  SCOPED_TRACE(test_case_file_name.c_str());

  ASSERT_TRUE(!layout_test_controller_.empty());

  // Creates a new cookie name. We will have to use a new cookie because
  // this function could be called multiple times.
  std::string status_cookie(kTestCompleteCookie);
  status_cookie += IntToString(test_count_);
  test_count_++;

  // Reads the layout test HTML file.
  FilePath test_file_path(layout_test_dir_);
  test_file_path = test_file_path.AppendASCII(test_case_file_name);
  std::string test_html;
  ASSERT_TRUE(file_util::ReadFileToString(test_file_path, &test_html));

  // Injects the layout test controller into the test HTML.
  test_html.insert(0, layout_test_controller_);
  ReplaceSubstringsAfterOffset(
      &test_html, 0, "%COOKIE%", status_cookie.c_str());

  // Creates the new layout test HTML file.
  FilePath new_test_file_path(new_layout_test_dir_);
  new_test_file_path = new_test_file_path.AppendASCII(test_case_file_name);
  ASSERT_TRUE(file_util::WriteFile(new_test_file_path,
                                   &test_html.at(0),
                                   static_cast<int>(test_html.size())));

  scoped_ptr<GURL> new_test_url;
  if (is_http_test)
    new_test_url.reset(new GURL(
        std::string("http://localhost:8080/") +
        WideToUTF8(test_case_dir_.ToWStringHack()) +
        "/" +
        test_case_file_name));
  else
    new_test_url.reset(new GURL(net::FilePathToFileURL(new_test_file_path)));

  // Runs the new layout test.
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(*new_test_url.get()));
  std::string escaped_value =
      WaitUntilCookieNonEmpty(tab.get(), *new_test_url.get(),
          status_cookie.c_str(), kTestIntervalMs, kTestWaitTimeoutMs);

  // Unescapes and normalizes the actual result.
  std::string value = UnescapeURLComponent(escaped_value,
      UnescapeRule::NORMAL | UnescapeRule::SPACES |
          UnescapeRule::URL_SPECIAL_CHARS | UnescapeRule::CONTROL_CHARS);
  value += "\n";
  ReplaceSubstringsAfterOffset(&value, 0, "\r", "");

  // Reads the expected result. First try to read from rebase directory.
  // If failed, read from original directory.
  std::string expected_result_value;
  if (!ReadExpectedResult(rebase_result_dir_,
                          test_case_file_name,
                          &expected_result_value)) {
    if (rebase_result_win_dir_.empty() ||
        !ReadExpectedResult(rebase_result_win_dir_,
                            test_case_file_name,
                            &expected_result_value))
      ReadExpectedResult(layout_test_dir_,
                         test_case_file_name,
                         &expected_result_value);
  }
  ASSERT_TRUE(!expected_result_value.empty());

  // Normalizes the expected result.
  ReplaceSubstringsAfterOffset(&expected_result_value, 0, "\r", "");

  // Compares the results.
  EXPECT_STREQ(expected_result_value.c_str(), value.c_str());
}

bool UILayoutTest::ReadExpectedResult(const FilePath& result_dir_path,
                                      const std::string test_case_file_name,
                                      std::string* expected_result_value) {
  FilePath expected_result_path(result_dir_path);
  expected_result_path = expected_result_path.AppendASCII(test_case_file_name);
  expected_result_path = expected_result_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("-expected"));
  expected_result_path =
      expected_result_path.ReplaceExtension(FILE_PATH_LITERAL("txt"));
  return file_util::ReadFileToString(expected_result_path,
                                     expected_result_value);
}
