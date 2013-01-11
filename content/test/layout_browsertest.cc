// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

#include <sstream>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_switches.h"
#include "content/shell/webkit_test_controller.h"
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

namespace content {
namespace {

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

}  // namespace

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
  FilePath src_dir;
  ASSERT_TRUE(PathService::Get(DIR_LAYOUT_TESTS, &src_dir));
  FilePath absolute_parent_dir = src_dir.Append(test_parent_dir_);
  ASSERT_TRUE(file_util::DirectoryExists(absolute_parent_dir));
  layout_test_dir_ = absolute_parent_dir.Append(test_case_dir_);
  ASSERT_TRUE(file_util::DirectoryExists(layout_test_dir_));

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
        absolute_parent_dir, port_));
    ASSERT_TRUE(test_http_server_->Start());
  }
}

void InProcessBrowserLayoutTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(switches::kDumpRenderTree);
}

void InProcessBrowserLayoutTest::SetUpOnMainThread() {
  test_controller_.reset(new WebKitTestController);
}

void InProcessBrowserLayoutTest::RunLayoutTest(
    const std::string& test_case_file_name) {
  GURL url = net::FilePathToFileURL(
      layout_test_dir_.AppendASCII(test_case_file_name));
  RunLayoutTestInternal(test_case_file_name, url);
}

void InProcessBrowserLayoutTest::RunHttpLayoutTest(
    const std::string& test_case_file_name) {
  DCHECK(test_http_server_.get());
  GURL url(StringPrintf(
      "http://127.0.0.1:%d/%s/%s", port_, test_case_dir_.MaybeAsASCII().c_str(),
      test_case_file_name.c_str()));
  RunLayoutTestInternal(test_case_file_name, url);
}

void InProcessBrowserLayoutTest::RunLayoutTestInternal(
    const std::string& test_case_file_name, const GURL& url) {
  std::stringstream result;
  scoped_ptr<WebKitTestResultPrinter> printer(
      new WebKitTestResultPrinter(&result, NULL));
  printer->set_capture_text_only(true);
  test_controller_->set_printer(printer.release());

  LOG(INFO) << "Navigating to URL " << url << " and blocking.";
  ASSERT_TRUE(
      test_controller_->PrepareForLayoutTest(url, layout_test_dir_, false, ""));
  base::RunLoop run_loop;
  run_loop.Run();
  LOG(INFO) << "Navigation completed.";
  ASSERT_TRUE(test_controller_->ResetAfterLayoutTest());

  std::string actual_text = result.str();;

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
      ReadExpectedResult(layout_test_dir_,
                         test_case_file_name,
                         &expected_text);
  }
  ASSERT_TRUE(!expected_text.empty());

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOutputLayoutTestDifferences)) {
    EXPECT_EQ(expected_text, actual_text) <<
        SaveResults(expected_text, actual_text);
  } else {
    EXPECT_EQ(expected_text, actual_text);
  }
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

}  // namespace content
