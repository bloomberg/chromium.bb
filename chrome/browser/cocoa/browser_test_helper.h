// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_TEST_HELPER_H_
#define CHROME_BROWSER_COCOA_BROWSER_TEST_HELPER_H_

#include "chrome/browser/browser.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/test/testing_profile.h"

// Base class which contains a valid Browser*.  Lots of boilerplate to
// recycle between unit test classes.
//
// TODO(jrg): move up a level (chrome/browser/cocoa -->
// chrome/browser), and use in non-Mac unit tests such as
// back_forward_menu_model_unittest.cc,
// navigation_controller_unittest.cc, ..
class BrowserTestHelper {
 public:
  BrowserTestHelper()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        file_thread_(ChromeThread::FILE, &message_loop_) {
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    profile_->BlockUntilBookmarkModelLoaded();
    browser_.reset(new Browser(Browser::TYPE_NORMAL, profile_.get()));
  }

  TestingProfile* profile() const { return profile_.get(); }
  Browser* browser() const { return browser_.get(); }

 private:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<Browser> browser_;
  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;
};

#endif  // CHROME_BROWSER_COCOA_BROWSER_TEST_HELPER_H_
