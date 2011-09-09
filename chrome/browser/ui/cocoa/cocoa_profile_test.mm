// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"

#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"

CocoaProfileTest::CocoaProfileTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      profile_manager_(static_cast<TestingBrowserProcess*>(g_browser_process)),
      profile_(NULL),
      file_thread_(new BrowserThread(BrowserThread::FILE, &message_loop_)),
      io_thread_(new BrowserThread(BrowserThread::IO, &message_loop_)) {
}

CocoaProfileTest::~CocoaProfileTest() {
  // Delete the testing profile on the UI thread. But first release the
  // browser, since it may trigger accesses to the profile upon destruction.
  browser_.reset();

  // Some services created on the TestingProfile require deletion on the UI
  // thread. If the scoper in TestingBrowserProcess, owned by ChromeTestSuite,
  // were to delete the ProfileManager, the UI thread would at that point no
  // longer exist.
  static_cast<TestingBrowserProcess*>(g_browser_process)->SetProfileManager(
      NULL);

  // Make sure any pending tasks run before we destroy other threads.
  message_loop_.RunAllPending();

  // Drop any new tasks for the IO and FILE threads.
  io_thread_.reset();
  file_thread_.reset();

  message_loop_.RunAllPending();
}

void CocoaProfileTest::SetUp() {
  CocoaTest::SetUp();

  ASSERT_TRUE(profile_manager_.SetUp());

  profile_ = profile_manager_.CreateTestingProfile("default");
  ASSERT_TRUE(profile_);

  profile_->CreateBookmarkModel(true);
  profile_->BlockUntilBookmarkModelLoaded();

  // TODO(shess): These are needed in case someone creates a browser
  // window off of browser_.  pkasting indicates that other
  // platforms use a stub |BrowserWindow| and thus don't need to do
  // this.
  // http://crbug.com/39725
  profile_->CreateAutocompleteClassifier();
  profile_->CreateTemplateURLService();

  browser_.reset(new Browser(Browser::TYPE_TABBED, profile_));
  ASSERT_TRUE(browser_.get());
}

void CocoaProfileTest::TearDown() {
  if (browser_.get() && browser_->window())
    CloseBrowserWindow();

  CocoaTest::TearDown();
}

BrowserWindow* CocoaProfileTest::CreateBrowserWindow() {
  browser_->InitBrowserWindow();
  return browser_->window();
}

void CocoaProfileTest::CloseBrowserWindow() {
  // Check to make sure a window was actually created.
  DCHECK(browser_->window());
  browser_->CloseAllTabs();
  browser_->CloseWindow();
  // |browser_| will be deleted by its BrowserWindowController.
  ignore_result(browser_.release());
}
