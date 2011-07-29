// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_UI_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/ui/webui/web_ui_test_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/test_navigation_observer.h"

class WebUIMessageHandler;

namespace base {
class Value;
}

// This macro simplifies the declaration of simple javascript unit tests.
// Use:
//   WEB_UI_UNITTEST_F(MyWebUIPageTest, myJavascriptUnittest);
#define WEB_UI_UNITTEST_F(x, y) \
  IN_PROC_BROWSER_TEST_F(x, y) { \
    ASSERT_TRUE(RunJavascriptTest(#y)); \
  }

// The runner of WebUI javascript based tests.
// See chrome/test/data/webui/test_api.js for the javascript side test API's.
//
// These tests should follow the form given in:
// chrome/test/data/webui/sample_downloads.js.
// and the lone test within this class.
class WebUIBrowserTest
    : public InProcessBrowserTest,
      public TestNavigationObserver::JsInjectionReadyObserver {
 public:
  typedef std::vector<const base::Value*> ConstValueVector;
  virtual ~WebUIBrowserTest();

  // Add a custom helper JS library for your test.
  // If a relative path is specified, it'll be read
  // as relative to the test data dir.
  void AddLibrary(const FilePath& library_path);

  // Runs a javascript function in the context of all libraries.
  // Note that calls to functions in test_api.js are not supported.
  bool RunJavascriptFunction(const std::string& function_name);
  bool RunJavascriptFunction(const std::string& function_name,
                             const base::Value& arg);
  bool RunJavascriptFunction(const std::string& function_name,
                             const base::Value& arg1,
                             const base::Value& arg2);
  bool RunJavascriptFunction(const std::string& function_name,
                             const ConstValueVector& function_arguments);

  // Runs a test fixture that may include calls to functions in test_api.js.
  bool RunJavascriptTestF(const std::string& test_fixture,
                          const std::string& test_name);

  // Runs a test that may include calls to functions in test_api.js.
  bool RunJavascriptTest(const std::string& test_name);
  bool RunJavascriptTest(const std::string& test_name,
                         const base::Value& arg);
  bool RunJavascriptTest(const std::string& test_name,
                         const base::Value& arg1,
                         const base::Value& arg2);
  bool RunJavascriptTest(const std::string& test_name,
                         const ConstValueVector& test_arguments);

  // Preloads the javascript libraries and sets the |libraries_preloaded| flag
  // to prevent re-loading at next javascript invocation. If
  // |override_chrome_send| is true, then chrome.send is overridden for
  // javascript to register handlers.
  void PreLoadJavascriptLibraries(const std::string& preload_test_fixture,
                                  const std::string& preload_test_name);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  void BrowsePreload(const GURL& browse_to,
                     const std::string& preload_test_fixture,
                     const std::string& preload_test_name);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  void BrowsePrintPreload(const GURL& browse_to,
                          const std::string& preload_test_fixture,
                          const std::string& preload_test_name);

 protected:
  WebUIBrowserTest();

  // Setup test path.
  virtual void SetUpInProcessBrowserTestFixture();

  // Returns a mock WebUI object under test (if any).
  virtual WebUIMessageHandler* GetMockMessageHandler();

  // Returns a file:// GURL constructed from |path| inside the test data dir for
  // webui tests.
  static GURL WebUITestDataPathToURL(const FilePath::StringType& path);

 private:
  // TestNavigationObserver::JsInjectionReadyObserver implementation.
  virtual void OnJsInjectionReady() OVERRIDE;

  // Builds a string containing all added javascript libraries.
  void BuildJavascriptLibraries(std::string* content);

  // Builds a string with a call to the runTest JS function, passing the
  // given test and its arguments.
  string16 BuildRunTestJSCall(const std::string& test_name,
                              const WebUIBrowserTest::ConstValueVector& args);

  // Calls the specified function with all libraries available. If |is_test|
  // is true, the framework listens for pass fail messages from javascript.
  // The provided arguments vector is passed to |function_name|.
  bool RunJavascriptUsingHandler(const std::string& function_name,
                                 const ConstValueVector& function_arguments,
                                 bool is_test);

  // Attaches mock and test handlers.
  void SetupHandlers();

  // Handles test framework messages.
  scoped_ptr<WebUITestHandler> test_handler_;

  // Location of test data (currently test/data/webui).
  FilePath test_data_directory_;

  // User added libraries
  std::vector<FilePath> user_libraries_;

  // Indicates that the libraries have been pre-loaded and to not load them
  // again.
  bool libraries_preloaded_;

  // Saves the states of |test_fixture| and |test_name| for calling
  // PreloadJavascriptLibraries().
  std::string preload_test_fixture_;
  std::string preload_test_name_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_UI_BROWSERTEST_H_
