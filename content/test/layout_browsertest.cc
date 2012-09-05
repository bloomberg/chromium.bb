// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_switches.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/layout_test_http_server.h"
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

namespace {

size_t FindInsertPosition(const std::string& html) {
  size_t tag_start = html.find("<html");
  if (tag_start == std::string::npos)
    return 0;
  size_t tag_end = html.find(">", tag_start);
  if (tag_end == std::string::npos)
    return 0;
  return tag_end + 1;
}

bool ReadExpectedResult(const FilePath& result_dir_path,
                        const std::string test_case_file_name,
                        std::string* expected_result_value) {
  FilePath expected_result_path(result_dir_path);
  expected_result_path = expected_result_path.AppendASCII(test_case_file_name);
  expected_result_path = expected_result_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("-expected"));
  expected_result_path =
      expected_result_path.ReplaceExtension(FILE_PATH_LITERAL("txt"));
  return file_util::ReadFileToString(
      expected_result_path, expected_result_value);
}

void ScrapeResultFromBrowser(content::Shell* window, std::string* actual_text) {
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
      window->web_contents()->GetRenderViewHost(),
      L"",
      L"window.domAutomationController.send(document.body.innerText);",
      actual_text));
}

static const std::string preamble =
      "\n<script>\n"
      "function TestRunner() {\n"
      "  this.wait_until_done_ = false;\n"
      "  this.dumpAsText = function () {};\n"
      "  this.waitUntilDone = function () {\n"
      "    this.wait_until_done_ = true;\n"
      "  }\n"
      "  this.notifyDone = function () {\n"
      "    document.title = 'done';\n"
      "  }\n"
      "  this.overridePreference = function () {}\n"
      "  this.OnEvent = function () {\n"
      "    if (!testRunner.wait_until_done_)\n"
      "      testRunner.notifyDone();\n"
      "  }\n"
      "  this.workerThreadCount = 0; \n"
      "}\n"
      "window.testRunner = new TestRunner();\n"
      "window.addEventListener('load', testRunner.OnEvent, false);\n"
      "</script>";

}

InProcessBrowserLayoutTest::InProcessBrowserLayoutTest(
    const FilePath& test_parent_dir, const FilePath& test_case_dir)
    : test_parent_dir_(test_parent_dir), test_case_dir_(test_case_dir),
      port_(-2) {
}

InProcessBrowserLayoutTest::InProcessBrowserLayoutTest(
    const FilePath& test_parent_dir, const FilePath& test_case_dir, int port)
    : test_parent_dir_(test_parent_dir), test_case_dir_(test_case_dir),
      port_(port) {
}

InProcessBrowserLayoutTest::~InProcessBrowserLayoutTest() {
  if (test_http_server_.get())
    CHECK(test_http_server_->Stop());
}

void InProcessBrowserLayoutTest::SetUpInProcessBrowserTestFixture() {
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  FilePath src_dir;
  ASSERT_TRUE(PathService::Get(content::DIR_LAYOUT_TESTS, &src_dir));
  our_original_layout_test_dir_ =
      src_dir.Append(test_parent_dir_).Append(test_case_dir_);
  ASSERT_TRUE(file_util::DirectoryExists(our_original_layout_test_dir_));
  our_layout_test_temp_dir_ = scoped_temp_dir_.path()
      .AppendASCII("LayoutTests").Append(test_parent_dir_);
  ASSERT_TRUE(file_util::CreateDirectory(
      our_layout_test_temp_dir_.Append(test_case_dir_)));
  file_util::CopyDirectory(
      our_original_layout_test_dir_.AppendASCII("resources"),
      our_layout_test_temp_dir_.Append(test_case_dir_).AppendASCII("resources"),
      true /*recursive*/);

  // Gets the file path to rebased expected result directory for the current
  // platform.
  //   $LayoutTestRoot/platform/chromium_***/...
  rebase_result_dir_ = src_dir.AppendASCII("platform");
  rebase_result_dir_ = rebase_result_dir_.AppendASCII(kPlatformName);
  rebase_result_dir_ = rebase_result_dir_.Append(test_parent_dir_);
  rebase_result_dir_ = rebase_result_dir_.Append(test_case_dir_);

  // Generic chromium expected results. Not OS-specific. For example,
  // v8-specific differences go here.
  // chrome/test/data/layout_tests/LayoutTests/platform/chromium/...
  rebase_result_chromium_dir_ = src_dir.AppendASCII("platform").
                                        AppendASCII("chromium").
                                        Append(test_parent_dir_).
                                        Append(test_case_dir_);

  // Gets the file path to rebased expected result directory under the
  // win32 platform. This is used by other non-win32 platform to use the same
  // rebased expected results.
#if !defined(OS_WIN)
  rebase_result_win_dir_ = src_dir.AppendASCII("platform").
                                   AppendASCII("chromium-win").
                                   Append(test_parent_dir_).
                                   Append(test_case_dir_);
#endif

  if (port_ != -2) {
    // Layout tests expect that the server points at the
    // LayoutTests\http\tests directory.
    if (port_ == -1)
      port_ = base::RandInt(1024, 65535);
    test_http_server_.reset(new LayoutTestHttpServer(
        our_layout_test_temp_dir_, port_));
    ASSERT_TRUE(test_http_server_->Start());
  }
}

void InProcessBrowserLayoutTest::RunLayoutTest(
    const std::string& test_case_file_name) {
  FilePath file_path;
  WriteModifiedFile(test_case_file_name, &file_path);
  GURL url = net::FilePathToFileURL(file_path);
  RunLayoutTestInternal(test_case_file_name, url);
}

void InProcessBrowserLayoutTest::RunHttpLayoutTest(
    const std::string& test_case_file_name) {
  DCHECK(test_http_server_.get());
  FilePath file_path;
  WriteModifiedFile(test_case_file_name, &file_path);

  GURL url(StringPrintf(
      "http://127.0.0.1:%d/%s/%s", port_, test_case_dir_.MaybeAsASCII().c_str(),
      test_case_file_name.c_str()));
  RunLayoutTestInternal(test_case_file_name, url);
}

void InProcessBrowserLayoutTest::RunLayoutTestInternal(
    const std::string& test_case_file_name, const GURL& url) {
  LOG(INFO) << "Navigating to URL " << url << " and blocking.";
  const string16 expected_title = ASCIIToUTF16("done");
  content::TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  content::NavigateToURL(shell(), url);
  LOG(INFO) << "Navigation completed, now waiting for title.";
  string16 final_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);

  std::string actual_text;
  ScrapeResultFromBrowser(shell(), &actual_text);
  ReplaceSubstringsAfterOffset(&actual_text, 0, "\r", "");
  TrimString(actual_text, "\n", &actual_text);

  std::string expected_text;
  // Reads the expected result. First try to read from rebase directory.
  // If failed, read from original directory.
  if (!ReadExpectedResult(rebase_result_dir_,
                          test_case_file_name,
                          &expected_text) &&
      !ReadExpectedResult(rebase_result_chromium_dir_,
                          test_case_file_name,
                          &expected_text)) {
    if (rebase_result_win_dir_.empty() ||
        !ReadExpectedResult(rebase_result_win_dir_,
                            test_case_file_name,
                            &expected_text))
      ReadExpectedResult(our_original_layout_test_dir_,
                         test_case_file_name,
                         &expected_text);
  }
  ASSERT_TRUE(!expected_text.empty());
  ReplaceSubstringsAfterOffset(&expected_text, 0, "\r", "");
  TrimString(expected_text, "\n", &expected_text);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOutputLayoutTestDifferences)) {
    EXPECT_EQ(expected_text, actual_text) <<
        SaveResults(expected_text, actual_text);
  } else {
    EXPECT_EQ(expected_text, actual_text);
  }
}

void InProcessBrowserLayoutTest::AddResourceForLayoutTest(
    const FilePath& parent_dir,
    const FilePath& resource_name) {
  FilePath source;
  ASSERT_TRUE(PathService::Get(content::DIR_LAYOUT_TESTS, &source));
  source = source.Append(parent_dir);
  source = source.Append(resource_name);

  ASSERT_TRUE(file_util::PathExists(source));

  FilePath dest_parent_dir = scoped_temp_dir_.path().
      AppendASCII("LayoutTests").Append(parent_dir);
  ASSERT_TRUE(file_util::CreateDirectory(dest_parent_dir));
  FilePath dest = dest_parent_dir.Append(resource_name);

  if (file_util::DirectoryExists(source)) {
    ASSERT_TRUE(file_util::CreateDirectory(dest.DirName()));
    ASSERT_TRUE(file_util::CopyDirectory(source, dest, true));
  } else {
    ASSERT_TRUE(file_util::CopyFile(source, dest));
  }
}

void InProcessBrowserLayoutTest::WriteModifiedFile(
    const std::string& test_case_file_name, FilePath* test_path) {
  FilePath path_to_single_test =
      our_original_layout_test_dir_.AppendASCII(test_case_file_name);
  std::string test_html;
  ASSERT_TRUE(file_util::ReadFileToString(path_to_single_test, &test_html));

  size_t insertion_position = FindInsertPosition(test_html);
  test_html.insert(insertion_position, preamble);
  *test_path = our_layout_test_temp_dir_.Append(test_case_dir_);
  *test_path = test_path->AppendASCII(test_case_file_name);
  ASSERT_TRUE(file_util::WriteFile(*test_path,
                                   &test_html.at(0),
                                   static_cast<int>(test_html.size())));
}

std::string InProcessBrowserLayoutTest::SaveResults(const std::string& expected,
                                                    const std::string& actual) {
  FilePath cwd;
  EXPECT_TRUE(file_util::CreateNewTempDirectory(FILE_PATH_LITERAL(""), &cwd));
  FilePath expected_filename = cwd.Append(FILE_PATH_LITERAL("expected.txt"));
  FilePath actual_filename = cwd.Append(FILE_PATH_LITERAL("actual.txt"));
  EXPECT_NE(-1, file_util::WriteFile(expected_filename,
                                     expected.c_str(),
                                     expected.size()));
  EXPECT_NE(-1, file_util::WriteFile(actual_filename,
                                     actual.c_str(),
                                     actual.size()));
  return StringPrintf("Wrote %"PRFilePath" %"PRFilePath,
                      expected_filename.value().c_str(),
                      actual_filename.value().c_str());
}
