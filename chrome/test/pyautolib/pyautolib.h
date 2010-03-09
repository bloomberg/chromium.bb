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

// The C++ style guide forbids using default arguments but I'm taking the
// liberty of allowing it in this file. The sole purpose of this (and the
// .cc) is to support the python interface, and default args are allowed in
// python. Strictly adhering to the guide here would mean having to re-define
// all methods in python just for the sake of providing default args. This
// seems cumbersome and unwanted.

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

  // Navigate to the given URL in the active tab. Blocks until page loaded.
  void NavigateToURL(const char* url_string);

  // Navigate to the given URL in given tab in the given window.
  // Blocks until page loaded.
  void NavigateToURL(const char* url_string, int window_index, int tab_index);

  // Get the URL of the active tab.
  GURL GetActiveTabURL(int window_index = 0);

  int GetTabCount(int window_index = 0);

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

  // Shows or hides the download shelf.
  void SetDownloadShelfVisible(bool is_visible, int window_index = 0);

  // Determines the visibility of the download shelf
  bool IsDownloadShelfVisible(int window_index = 0);

  // Open the Find box
  void OpenFindInPage(int window_index = 0);

  // Determines the visibility of the Find box
  bool IsFindInPageVisible(int window_index = 0);

  // Get the path to the downloads directory
  FilePath GetDownloadDirectory();

  // AutomationProxy methods

  // Open a new browser window. Returns false on failure.
  bool OpenNewBrowserWindow(bool show);

  // Installs the extension crx. Returns true only if extension was installed
  // and loaded successfully. Overinstalls will fail.
  bool InstallExtension(const FilePath& crx_file);

  // Returns bookmark bar visibility state.
  bool GetBookmarkBarVisibility();

  // Returns bookmark bar animation state.  Warning: timing issues may
  // change this return value unexpectedly.
  bool IsBookmarkBarAnimating();

  // Wait for the bookmark bar animation to complete.
  // If |wait_for_open| is true, wait for it to open.
  // If |wait_for_open| is false, wait for it to close.
  bool WaitForBookmarkBarVisibilityChange(bool wait_for_open);

 private:
  base::ScopedNSAutoreleasePool pool_;
};

#endif  // CHROME_TEST_PYAUTOLIB_PYAUTOLIB_H_

