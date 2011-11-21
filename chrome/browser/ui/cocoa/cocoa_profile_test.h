// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILE_TEST_H_
#define CHROME_BROWSER_UI_COCOA_PROFILE_TEST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/test/test_browser_thread.h"

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
class CocoaProfileTest : public CocoaTest {
 public:
  CocoaProfileTest();
  virtual ~CocoaProfileTest();

  // This constructs a a Browser and a TestingProfile. It is guaranteed to
  // succeed, else it will ASSERT and cause the test to fail. Subclasses that
  // do work in SetUp should ASSERT that either browser() or profile() are
  // non-NULL before proceeding after the call to super (this).
  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  TestingProfileManager* testing_profile_manager() {
    return &profile_manager_;
  }
  TestingProfile* profile() { return profile_; }
  Browser* browser() { return browser_.get(); }

  // Creates the browser window. To close this window call |CloseBrowserWindow|.
  // Do NOT call close directly on the window.
  BrowserWindow* CreateBrowserWindow();

  // Closes the window for this browser. This must only be called after
  // CreateBrowserWindow(). This will automatically be called as part of
  // TearDown() if it's not been done already.
  void CloseBrowserWindow();

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  TestingProfileManager profile_manager_;
  TestingProfile* profile_;  // Weak; owned by profile_manager_.
  scoped_ptr<Browser> browser_;

  scoped_ptr<content::TestBrowserThread> file_thread_;
  scoped_ptr<content::TestBrowserThread> io_thread_;
};

#endif  // CHROME_BROWSER_UI_COCOA_PROFILE_TEST_H_
