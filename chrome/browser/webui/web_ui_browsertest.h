// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_WEB_UI_BROWSERTEST_H_
#define CHROME_BROWSER_WEBUI_WEB_UI_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/webui/web_ui_handler_browsertest.h"
#include "chrome/test/in_process_browser_test.h"

class WebUIMessageHandler;

// The runner of WebUI javascript based tests.
// See chrome/test/data/dom_ui/test_api.js for the javascript side test API's.
//
// These tests should follow the form given in:
// chrome/test/data/dom_ui/sample_downloads.js.
// and the lone test within this class.
class WebUIBrowserTest : public InProcessBrowserTest {
 public:
  virtual ~WebUIBrowserTest();

  bool RunWebUITest(const FilePath::CharType* src_path);

 protected:
  WebUIBrowserTest();

  // Setup test path.
  virtual void SetUpInProcessBrowserTestFixture();

  // Returns a mock WebUI object under test (if any).
  virtual WebUIMessageHandler* GetMockMessageHandler();

 private:
  // Builds a javascript test in the form:
  // <js_library> ...
  // <src_path> ...
  //   runTests(function test1() {...},
  //      ...
  //   );
  void BuildJavaScriptTest(const FilePath& src_path,
                           std::string* content);

  // Attaches mock and test handlers.
  void SetupHandlers();

  // Handles test framework messages.
  scoped_ptr<WebUIHandlerBrowserTest> test_handler_;

  // Location of test data (currently test/data/dom_ui).
  FilePath test_data_directory_;
};

#endif  // CHROME_BROWSER_WEBUI_WEB_UI_BROWSERTEST_H_
