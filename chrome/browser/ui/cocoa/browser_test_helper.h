// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_TEST_HELPER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_TEST_HELPER_H_
#pragma once

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/testing_profile.h"

// Base class which contains a valid Browser*.  Lots of boilerplate to
// recycle between unit test classes.
//
// This class creates fake UI, file, and IO threads because many objects that
// are attached to the TestingProfile (and other objects) have traits that limit
// their destruction to certain threads. For example, the net::URLRequestContext
// can only be deleted on the IO thread; without this fake IO thread, the object
// would never be deleted and would report as a leak under Valgrind. Note that
// these are fake threads and they all share the same MessageLoop.
//
// TODO(jrg): move up a level (chrome/browser/ui/cocoa -->
// chrome/browser), and use in non-Mac unit tests such as
// back_forward_menu_model_unittest.cc,
// navigation_controller_unittest.cc, ..
class BrowserTestHelper {
 public:
  BrowserTestHelper();
  virtual ~BrowserTestHelper();

  virtual TestingProfile* profile() const;
  Browser* browser() const { return browser_.get(); }

  // Creates the browser window. To close this window call |CloseBrowserWindow|.
  // Do NOT call close directly on the window.
  BrowserWindow* CreateBrowserWindow();

  // Closes the window for this browser. This must only be called after
  // CreateBrowserWindow().
  void CloseBrowserWindow();

 private:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  scoped_ptr<BrowserThread> file_thread_;
  scoped_ptr<BrowserThread> io_thread_;
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_TEST_HELPER_H_
