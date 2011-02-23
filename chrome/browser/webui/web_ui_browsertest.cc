// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/web_ui_browsertest.h"

#include "base/path_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "ui/base/resource/resource_bundle.h"

static const FilePath::CharType* kWebUILibraryJS =
    FILE_PATH_LITERAL("test_api.js");
static const FilePath::CharType* kWebUITestFolder = FILE_PATH_LITERAL("webui");

WebUIBrowserTest::~WebUIBrowserTest() {}

bool WebUIBrowserTest::RunWebUITest(const FilePath::CharType* src_path) {
  std::string content;
  BuildJavaScriptTest(FilePath(src_path), &content);
  SetupHandlers();
  return test_handler_->Execute(content);
}

WebUIBrowserTest::WebUIBrowserTest()
    : test_handler_(new WebUIHandlerBrowserTest()) {}

void WebUIBrowserTest::SetUpInProcessBrowserTestFixture() {
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_));
  test_data_directory_ = test_data_directory_.Append(kWebUITestFolder);

  // TODO(dtseng): should this be part of every BrowserTest or just WebUI test.
  FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::AddDataPackToSharedInstance(resources_pack_path);
}

WebUIMessageHandler* WebUIBrowserTest::GetMockMessageHandler() {
  return NULL;
}

void WebUIBrowserTest::BuildJavaScriptTest(const FilePath& src_path,
                                           std::string* content) {
  ASSERT_TRUE(content != NULL);
  std::string library_content, src_content;
  ASSERT_TRUE(file_util::ReadFileToString(
      test_data_directory_.Append(FilePath(kWebUILibraryJS)),
          &library_content));
  ASSERT_TRUE(file_util::ReadFileToString(
      test_data_directory_.Append(src_path), &src_content));

  content->append(library_content);
  content->append(";\n");
  content->append(src_content);
}

void WebUIBrowserTest::SetupHandlers() {
  WebUI* web_ui_instance =
      browser()->GetSelectedTabContents()->web_ui();
  ASSERT_TRUE(web_ui_instance != NULL);
  web_ui_instance->register_callback_overwrites(true);
  test_handler_->Attach(web_ui_instance);

  if (GetMockMessageHandler())
    GetMockMessageHandler()->Attach(web_ui_instance);
}

IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, TestSamplePass) {
  // Navigate to UI.
  // TODO(dtseng): make accessor for subclasses to return?
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));

  ASSERT_TRUE(RunWebUITest(FILE_PATH_LITERAL("sample_downloads.js")));
}
