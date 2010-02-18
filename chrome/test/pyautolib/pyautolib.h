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

  // Get the URL of the active tab.
  GURL GetActiveTabURL();

  // BrowserProxy methods

  // Shows or hides the download shelf.
  void SetShelfVisible(bool is_visible);

  // Appends a new tab with the given URL in the given or first browser window.
  bool AppendTab(const GURL& tab_url, int window_index = 0);

  // Activate the tab at the given zero-based index in the given or first
  // browser window.
  bool ActivateTab(int tab_index, int window_index = 0);

  // Apply the accelerator with given id (IDC_BACK, IDC_NEWTAB ...) to the
  // browser window at the given or first index.
  // The list can be found at chrome/app/chrome_dll_resource.h
  // Returns true if the call was successful.
  bool ApplyAccelerator(int id, int window_index = 0);

  // Determines the visibility of the download shelf
  bool IsShelfVisible();

  // Open the Find box
  void OpenFindInPage();

  // Determines the visibility of the Find box
  bool IsFindInPageVisible();

  // Get the path to the downloads directory
  std::string GetDownloadDirectory();

  // AutomationProxy methods

  // Open a new browser window. Returns false on failure.
  bool OpenNewBrowserWindow(bool show);

  // Installs the extension crx. Returns true only if extension was installed
  // and loaded successfully. Overinstalls will fail.
  bool InstallExtension(const FilePath& crx_file);

 private:
  base::ScopedNSAutoreleasePool pool_;
};

#endif  // CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_

