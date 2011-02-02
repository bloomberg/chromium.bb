// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_BROWSERTEST_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_BROWSERTEST_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/dom_ui/dom_ui_handler_browsertest.h"
#include "chrome/test/in_process_browser_test.h"

// The runner of DOMUI javascript based tests.
// See chrome/test/data/dom_ui/test_api.js for the javascript side test API's.
//
// These tests should follow the form given in:
// chrome/test/data/dom_ui/sample_downloads.js.
// and the lone test within this class.
class DOMUITest : public InProcessBrowserTest {
 public:
  bool RunDOMUITest(const FilePath::CharType* src_path);

 protected:
  DOMUITest();

  virtual void SetUpInProcessBrowserTestFixture();

 private:
  // Builds a javascript test in the form:
  // <js_library> ...
  // <src_path> ...
  //   runTests(function test1() {...},
  //      ...
  //   );
  void BuildJavaScriptTest(const FilePath& src_path,
                           std::string* content);

  // Handles test framework messages.
  scoped_ptr<DOMUITestHandler> handler_;

  // Location of test data (currently test/data/dom_ui).
  FilePath test_data_directory_;
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_BROWSERTEST_H_
