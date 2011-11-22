// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_layout_test.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/tab_proxy.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
static const char kPlatformName[] = "chromium-win";
#elif defined(OS_MACOSX)
static const char kPlatformName[] = "chromium-mac";
#elif defined(OS_LINUX)
static const char kPlatformName[] = "chromium-linux";
#elif defined(OS_OPENBSD)
static const char kPlatformName[] = "chromium-openbsd";
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
                                           int port) {
  FilePath src_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_LAYOUT_TESTS, &src_dir));
  layout_test_dir_ = src_dir.Append(test_parent_dir);
  layout_test_dir_ = layout_test_dir_.Append(test_case_dir);
  ASSERT_TRUE(file_util::DirectoryExists(layout_test_dir_));

  // Gets the file path to rebased expected result directory for the current
  // platform.
  //   $LayoutTestRoot/platform/chromium_***/...
  rebase_result_dir_ = src_dir.AppendASCII("platform");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII(kPlatformName);
  rebase_result_dir_ = rebase_result_dir_.Append(test_parent_dir);
  rebase_result_dir_ = rebase_result_dir_.Append(test_case_dir);

  // Generic chromium expected results. Not OS-specific. For example,
  // v8-specific differences go here.
  // chrome/test/data/layout_tests/LayoutTests/platform/chromium/...
  rebase_result_chromium_dir_ = src_dir.AppendASCII("platform")
                                       .AppendASCII("chromium")
                                       .Append(test_parent_dir)
                                       .Append(test_case_dir);

  // Gets the file path to rebased expected result directory under the
  // win32 platform. This is used by other non-win32 platform to use the same
  // rebased expected results.
#if !defined(OS_WIN)
  rebase_result_win_dir_ = src_dir.AppendASCII("platform")
                                  .AppendASCII("chromium-win")
                                  .Append(test_parent_dir)
                                  .Append(test_case_dir);
#endif

  // Creates the temporary directory.
  ASSERT_TRUE(file_util::CreateNewTempDirectory(
      FILE_PATH_LITERAL("chrome_ui_layout_tests_"), &temp_test_dir_));

  // Creates the new layout test subdirectory under the temp directory.
  // Note that we have to mimic the same layout test directory structure,
  // like .../LayoutTests/fast/workers/.... Otherwise those layout tests
  // dealing with location property, like worker-location.html, could fail.
  new_layout_test_dir_ = temp_test_dir_;
  new_layout_test_dir_ = new_layout_test_dir_.AppendASCII("LayoutTests");
  new_layout_test_dir_ = new_layout_test_dir_.Append(test_parent_dir);
  if (port == kHttpPort) {
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
  if (port == kHttpPort) {
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
  layout_test_controller_.clear();
  ASSERT_TRUE(file_util::ReadFileToString(path, &layout_test_controller_));
}

void UILayoutTest::AddResourceForLayoutTest(const FilePath& parent_dir,
                                            const FilePath& resource_name) {
  FilePath source;
  ASSERT_TRUE(PathService::Get(chrome::DIR_LAYOUT_TESTS, &source));
  source = source.Append(parent_dir);
  source = source.Append(resource_name);

  ASSERT_TRUE(file_util::PathExists(source));

  FilePath dest_parent_dir = temp_test_dir_.
      AppendASCII("LayoutTests").Append(parent_dir);
  ASSERT_TRUE(file_util::CreateDirectory(dest_parent_dir));
  FilePath dest = dest_parent_dir.Append(resource_name);

  if (file_util::DirectoryExists(source)) {
    ASSERT_TRUE(file_util::CopyDirectory(source, dest, true));
  } else {
    ASSERT_TRUE(file_util::CopyFile(source, dest));
  }
}

static size_t FindInsertPosition(const std::string& html) {
  size_t tag_start = html.find("<html");
  if (tag_start == std::string::npos)
    return 0;
  size_t tag_end = html.find(">", tag_start);
  if (tag_end == std::string::npos)
    return 0;
  return tag_end + 1;
}

void UILayoutTest::RunLayoutTest(const std::string& test_case_file_name,
                                 int port) {
  base::Time start = base::Time::Now();
  SCOPED_TRACE(test_case_file_name.c_str());

  ASSERT_TRUE(!layout_test_controller_.empty());

  // Creates a new cookie name. We will have to use a new cookie because
  // this function could be called multiple times.
  std::string status_cookie(kTestCompleteCookie);
  status_cookie += base::IntToString(test_count_);
  test_count_++;

  // Reads the layout test HTML file.
  FilePath test_file_path(layout_test_dir_);
  test_file_path = test_file_path.AppendASCII(test_case_file_name);
  std::string test_html;
  ASSERT_TRUE(file_util::ReadFileToString(test_file_path, &test_html));

  // Injects the layout test controller into the test HTML.
  size_t insertion_position = FindInsertPosition(test_html);
  test_html.insert(insertion_position, layout_test_controller_);
  ReplaceFirstSubstringAfterOffset(
      &test_html, insertion_position, "%COOKIE%", status_cookie.c_str());

  // Creates the new layout test HTML file.
  FilePath new_test_file_path(new_layout_test_dir_);
  new_test_file_path = new_test_file_path.AppendASCII(test_case_file_name);
  ASSERT_TRUE(file_util::WriteFile(new_test_file_path,
                                   &test_html.at(0),
                                   static_cast<int>(test_html.size())));
  // We expect the test case dir to be ASCII.  It might be empty, so we
  // can't test whether MaybeAsASCII succeeded, but the tests will fail
  // loudly in that case anyway.
  std::string url_path = test_case_dir_.MaybeAsASCII();

  scoped_ptr<GURL> new_test_url;
  if (port != kNoHttpPort)
    new_test_url.reset(new GURL(
        StringPrintf("http://127.0.0.1:%d/", port) +
        url_path + "/" + test_case_file_name));
  else
    new_test_url.reset(new GURL(net::FilePathToFileURL(new_test_file_path)));

  // Runs the new layout test.
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(*new_test_url.get()));
  std::string escaped_value =
      WaitUntilCookieNonEmpty(tab.get(), *new_test_url.get(),
          status_cookie.c_str(), TestTimeouts::action_max_timeout_ms());

  // Unescapes and normalizes the actual result.
  std::string value = net::UnescapeURLComponent(escaped_value,
      net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
      net::UnescapeRule::URL_SPECIAL_CHARS | net::UnescapeRule::CONTROL_CHARS);
  value += "\n";
  ReplaceSubstringsAfterOffset(&value, 0, "\r", "");

  // Reads the expected result. First try to read from rebase directory.
  // If failed, read from original directory.
  std::string expected_result_value;
  if (!ReadExpectedResult(rebase_result_dir_,
                          test_case_file_name,
                          &expected_result_value) &&
      !ReadExpectedResult(rebase_result_chromium_dir_,
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

  VLOG(1) << "Test " << test_case_file_name
          << " took " << (base::Time::Now() - start).InMilliseconds() << "ms";
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
