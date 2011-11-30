// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_UI_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/string16.h"
#include "chrome/browser/ui/webui/web_ui_test_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/js_injection_ready_observer.h"
#include "chrome/test/base/test_navigation_observer.h"

class RenderViewHost;
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
      public JsInjectionReadyObserver {
 public:
  typedef std::vector<const base::Value*> ConstValueVector;
  virtual ~WebUIBrowserTest();

  // Add a custom helper JS library for your test.
  // If a relative path is specified, it'll be read
  // as relative to the test data dir.
  void AddLibrary(const FilePath& library_path);

  // Runs a javascript function in the context of all libraries.
  // Note that calls to functions in test_api.js are not supported.
  // Takes ownership of Value arguments.
  bool RunJavascriptFunction(const std::string& function_name);
  bool RunJavascriptFunction(const std::string& function_name,
                             base::Value* arg);
  bool RunJavascriptFunction(const std::string& function_name,
                             base::Value* arg1,
                             base::Value* arg2);
  bool RunJavascriptFunction(const std::string& function_name,
                             const ConstValueVector& function_arguments);

  // Runs a test fixture that may include calls to functions in test_api.js.
  bool RunJavascriptTestF(bool is_async,
                          const std::string& test_fixture,
                          const std::string& test_name);

  // Runs a test that may include calls to functions in test_api.js.
  // Takes ownership of Value arguments.
  bool RunJavascriptTest(const std::string& test_name);
  bool RunJavascriptTest(const std::string& test_name,
                         base::Value* arg);
  bool RunJavascriptTest(const std::string& test_name,
                         base::Value* arg1,
                         base::Value* arg2);
  bool RunJavascriptTest(const std::string& test_name,
                         const ConstValueVector& test_arguments);

  // Runs a test that may include calls to functions in test_api.js, and waits
  // for call to testDone().  Takes ownership of Value arguments.
  bool RunJavascriptAsyncTest(const std::string& test_name);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              base::Value* arg);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              base::Value* arg1,
                              base::Value* arg2);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              base::Value* arg1,
                              base::Value* arg2,
                              base::Value* arg3);
  bool RunJavascriptAsyncTest(const std::string& test_name,
                              const ConstValueVector& test_arguments);

  // Sends message through |preload_host| to preload javascript libraries and
  // sets the |libraries_preloaded| flag to prevent re-loading at next
  // javascript invocation.
  void PreLoadJavascriptLibraries(const std::string& preload_test_fixture,
                                  const std::string& preload_test_name,
                                  RenderViewHost* preload_host);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  void BrowsePreload(const GURL& browse_to);

  // Called by javascript-generated test bodies to browse to a page and preload
  // the javascript for the given |preload_test_fixture| and
  // |preload_test_name|. chrome.send will be overridden to allow javascript
  // handler mocking.
  void BrowsePrintPreload(const GURL& browse_to);

 protected:
  // URL to dummy WebUI page for testing framework.
  static const char kDummyURL[];

  WebUIBrowserTest();

  // Accessors for preload test fixture and name.
  void set_preload_test_fixture(const std::string& preload_test_fixture);
  void set_preload_test_name(const std::string& preload_test_name);

  // Set up & tear down console error catching.
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

  // Set up test path & override for |kDummyURL|.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // Tear down override for |kDummyURL|.
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

  // Set a WebUI instance to run tests on.
  void SetWebUIInstance(WebUI* web_ui);

  // Returns a mock WebUI object under test (if any).
  virtual WebUIMessageHandler* GetMockMessageHandler();

  // Returns a file:// GURL constructed from |path| inside the test data dir for
  // webui tests.
  static GURL WebUITestDataPathToURL(const FilePath::StringType& path);

 private:
  // JsInjectionReadyObserver implementation.
  virtual void OnJsInjectionReady(RenderViewHost* render_view_host) OVERRIDE;

  // Builds a string containing all added javascript libraries.
  void BuildJavascriptLibraries(string16* content);

  // Builds a string with a call to the runTest JS function, passing the
  // given |is_async|, |test_name| and its |args|.
  string16 BuildRunTestJSCall(bool is_async,
                              const std::string& test_name,
                              const WebUIBrowserTest::ConstValueVector& args);

  // Loads all libraries added with AddLibrary(), and calls |function_name| with
  // |function_arguments|. When |is_test| is true, the framework wraps
  // |function_name| with a test helper function, which waits for completion,
  // logging an error message on failure, otherwise |function_name| is called
  // asynchronously. When |preload_host| is non-NULL, sends the javascript to
  // the RenderView for evaluation at the appropriate time before the onload
  // call is made. Passes |is_async| along to runTest wrapper.
  bool RunJavascriptUsingHandler(const std::string& function_name,
                                 const ConstValueVector& function_arguments,
                                 bool is_test,
                                 bool is_async,
                                 RenderViewHost* preload_host);

  // Attaches mock and test handlers.
  void SetupHandlers();

  // Handles test framework messages.
  scoped_ptr<WebUITestHandler> test_handler_;

  // Location of test data (currently test/data/webui).
  FilePath test_data_directory_;

  // Location of generated test data (<(PROGRAM_DIR)/test_data).
  FilePath gen_test_data_directory_;

  // User added libraries
  std::vector<FilePath> user_libraries_;

  // Indicates that the libraries have been pre-loaded and to not load them
  // again.
  bool libraries_preloaded_;

  // Saves the states of |test_fixture| and |test_name| for calling
  // PreloadJavascriptLibraries().
  std::string preload_test_fixture_;
  std::string preload_test_name_;

  // When this is non-NULL, this is The WebUI instance used for testing.
  // Otherwise the selected tab's web_ui is used.
  WebUI* override_selected_web_ui_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_UI_BROWSERTEST_H_
