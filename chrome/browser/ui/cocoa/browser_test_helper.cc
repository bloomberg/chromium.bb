// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/browser_test_helper.h"

BrowserTestHelper::BrowserTestHelper()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(new BrowserThread(BrowserThread::FILE, &message_loop_)),
      io_thread_(new BrowserThread(BrowserThread::IO, &message_loop_)) {
  profile_.reset(new TestingProfile());
  profile_->CreateBookmarkModel(true);
  profile_->BlockUntilBookmarkModelLoaded();

  // TODO(shess): These are needed in case someone creates a browser
  // window off of browser_.  pkasting indicates that other
  // platforms use a stub |BrowserWindow| and thus don't need to do
  // this.
  // http://crbug.com/39725
  profile_->CreateAutocompleteClassifier();
  profile_->CreateTemplateURLModel();

  browser_.reset(new Browser(Browser::TYPE_NORMAL, profile_.get()));
}

BrowserTestHelper::~BrowserTestHelper() {
  // Delete the testing profile on the UI thread. But first release the
  // browser, since it may trigger accesses to the profile upon destruction.
  browser_.reset();
  message_loop_.DeleteSoon(FROM_HERE, profile_.release());

  // Make sure any pending tasks run before we destroy other threads.
  message_loop_.RunAllPending();

  // Drop any new tasks for the IO and FILE threads.
  io_thread_.reset();
  file_thread_.reset();

  message_loop_.RunAllPending();
}

TestingProfile* BrowserTestHelper::profile() const {
  return profile_.get();
}

BrowserWindow* BrowserTestHelper::CreateBrowserWindow() {
  browser_->InitBrowserWindow();
  return browser_->window();
}

void BrowserTestHelper::CloseBrowserWindow() {
  // Check to make sure a window was actually created.
  DCHECK(browser_->window());
  browser_->CloseAllTabs();
  browser_->CloseWindow();
  // |browser_| will be deleted by its BrowserWindowController.
  ignore_result(browser_.release());
}
