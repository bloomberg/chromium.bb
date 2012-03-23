// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/content_paths.h"
#include "net/base/net_util.h"

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

void ReadExpectedResult(const FilePath& result_dir_path,
                        const std::string test_case_file_name,
                        std::string* expected_result_value) {
  FilePath expected_result_path(result_dir_path);
  expected_result_path = expected_result_path.AppendASCII(test_case_file_name);
  expected_result_path = expected_result_path.InsertBeforeExtension(
      FILE_PATH_LITERAL("-expected"));
  expected_result_path =
      expected_result_path.ReplaceExtension(FILE_PATH_LITERAL("txt"));
  std::string raw_result;
  EXPECT_TRUE(file_util::ReadFileToString(expected_result_path, &raw_result));
  TrimString(raw_result, "\n", expected_result_value);
}

void ScrapeResultFromBrowser(Browser* browser, std::string* actual_text) {
  DOMElementProxyRef doc = ui_test_utils::GetActiveDOMDocument(browser);
  DOMElementProxyRef body =
      doc->FindElement(DOMElementProxy::By::XPath("//body"));
  ASSERT_TRUE(body.get());
  ASSERT_TRUE(body->GetInnerText(actual_text));
}

static const std::string preamble =
      "\n<script>\n"
      "function LayoutTestController() {\n"
      "  this.dumpAsText = function () {};\n"
      "  this.waitUntilDone = function () {};\n"
      "  this.notifyDone = function () {\n"
      "    document.title = 'done';\n"
      "  }\n"
      "}\n"
      "window.layoutTestController = new LayoutTestController();\n"
      "</script>";

}

InProcessBrowserLayoutTest::InProcessBrowserLayoutTest(
    const FilePath relative_layout_test_path)
        : original_relative_path_(relative_layout_test_path) {
  EnableDOMAutomation();
}

InProcessBrowserLayoutTest::~InProcessBrowserLayoutTest() {}

void InProcessBrowserLayoutTest::SetUpInProcessBrowserTestFixture() {
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  FilePath ThirdParty_WebKit_LayoutTests;
  ASSERT_TRUE(PathService::Get(content::DIR_LAYOUT_TESTS,
                               &ThirdParty_WebKit_LayoutTests));
  our_original_layout_test_dir_ =
      ThirdParty_WebKit_LayoutTests.Append(original_relative_path_);
  ASSERT_TRUE(file_util::DirectoryExists(our_original_layout_test_dir_));
  our_layout_test_temp_dir_ = scoped_temp_dir_.path()
      .AppendASCII("LayoutTests").Append(original_relative_path_);
  ASSERT_TRUE(file_util::CreateDirectory(our_layout_test_temp_dir_));
  file_util::CopyDirectory(
      our_original_layout_test_dir_.AppendASCII("resources"),
      our_layout_test_temp_dir_.AppendASCII("resources"),
      true /*recursive*/);
}

void InProcessBrowserLayoutTest::RunLayoutTest(
    const std::string& test_case_file_name) {
  GURL test_url;
  WriteModifiedFile(test_case_file_name, &test_url);

  LOG(INFO) << "Navigating to URL " << test_url << " and blocking.";
  const string16 expected_title = ASCIIToUTF16("done");
  ui_test_utils::TitleWatcher title_watcher(
      browser()->GetSelectedWebContents(), expected_title);
  ui_test_utils::NavigateToURL(browser(), test_url);
  string16 final_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);

  std::string actual_text;
  ScrapeResultFromBrowser(browser(), &actual_text);

  std::string expected_text;
  ReadExpectedResult(our_original_layout_test_dir_, test_case_file_name,
                     &expected_text);

  EXPECT_EQ(expected_text, actual_text);
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
    ASSERT_TRUE(file_util::CopyDirectory(source, dest, true));
  } else {
    ASSERT_TRUE(file_util::CopyFile(source, dest));
  }
}

void InProcessBrowserLayoutTest::WriteModifiedFile(
    const std::string& test_case_file_name, GURL* test_url) {
  FilePath path_to_single_test =
      our_original_layout_test_dir_.AppendASCII(test_case_file_name);
  std::string test_html;
  ASSERT_TRUE(file_util::ReadFileToString(path_to_single_test, &test_html));

  size_t insertion_position = FindInsertPosition(test_html);
  test_html.insert(insertion_position, preamble);
  FilePath new_test_file_path(our_layout_test_temp_dir_);
  new_test_file_path = new_test_file_path.AppendASCII(test_case_file_name);
  ASSERT_TRUE(file_util::WriteFile(new_test_file_path,
                                   &test_html.at(0),
                                   static_cast<int>(test_html.size())));
  *test_url = net::FilePathToFileURL(new_test_file_path);
}
