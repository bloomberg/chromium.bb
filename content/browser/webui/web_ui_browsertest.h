// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_BROWSERTEST_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/test/in_process_browser_test.h"
#include "content/browser/webui/web_ui_handler_browsertest.h"

class WebUIMessageHandler;

// The runner of WebUI javascript based tests.
// See chrome/test/data/webui/test_api.js for the javascript side test API's.
//
// These tests should follow the form given in:
// chrome/test/data/webui/sample_downloads.js.
// and the lone test within this class.
class WebUIBrowserTest : public InProcessBrowserTest {
 public:
  virtual ~WebUIBrowserTest();

  // Add a custom helper JS library for your test.
  void AddLibrary(const FilePath::CharType* library_path);

  // Runs a javascript function in the context of all libraries.
  // Note that calls to functions in test_api.js are not supported.
  bool RunJavascriptFunction(const std::string& function_name);

  // Runs a test that may include calls to functions in test_api.js.
  bool RunJavascriptTest(const std::string& test_name);

 protected:
  WebUIBrowserTest();

  // Setup test path.
  virtual void SetUpInProcessBrowserTestFixture();

  // Returns a mock WebUI object under test (if any).
  virtual WebUIMessageHandler* GetMockMessageHandler();

 private:
  // Builds a string containing all added javascript libraries.
  void BuildJavascriptLibraries(std::string* content);

  // Calls the specified function with all libraries available. If |is_test|
  // is true, the framework listens for pass fail messages from javascript.
  bool RunJavascriptUsingHandler(const std::string& function_name,
                                 bool is_test);

  // Attaches mock and test handlers.
  void SetupHandlers();

  // Handles test framework messages.
  scoped_ptr<WebUIHandlerBrowserTest> test_handler_;

  // Location of test data (currently test/data/webui).
  FilePath test_data_directory_;

  // User added libraries
  std::vector<FilePath> user_libraries;
};

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_BROWSERTEST_H_
