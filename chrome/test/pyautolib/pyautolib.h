// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares the C++ side of PyAuto, the python interface to
// Chromium automation.  It access Chromium's internals using Automation Proxy.

#ifndef CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_
#define CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_

#include "base/scoped_nsautorelease_pool.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui/ui_test_suite.h"

// TODO(nirnimesh): separate out the UITestSuite and UITestBase parts
//                  crbug.com/32292

// The primary class that interfaces with Automation Proxy.
// This class is accessed from python using swig.
class PyUITestSuite : public UITestSuite, public UITestBase {
 public:
  // Only public methods are accessible from swig.
  PyUITestSuite(int argc, char** argv);
  ~PyUITestSuite();

  // SetUp,TearDown is redeclared as public to make it accessible from swig.
  virtual void SetUp();
  virtual void TearDown();

  void NavigateToURL(const char* url_string);

  // Get the title of the active tab. Empty string in case of error.
  std::wstring GetActiveTabTitle();

  // BrowserProxy methods

  // Shows or hides the download shelf.
  void SetShelfVisible(bool is_visible);

  // Determines the visibility of the download shelf
  bool IsShelfVisible();

  // Open the Find box
  void OpenFindInPage();

  // Determines the visibility of the Find box
  bool IsFindInPageVisible();

 private:
  base::ScopedNSAutoreleasePool pool_;
};

#endif  // CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_

