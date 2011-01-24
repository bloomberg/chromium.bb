// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_UNITTEST_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_UNITTEST_H_

#import <objc/objc-runtime.h>
#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/test/model_test_utils.h"
#include "testing/platform_test.h"

class BookmarkModel;

// The fake object that acts as our app's delegate, useful for testing purposes.
@interface FakeAppDelegate : AppController {
 @public
  BrowserTestHelper* helper_;  // weak.
}
@property(nonatomic) BrowserTestHelper* helper;
// Return the |TestingProfile*| which is used for testing.
- (Profile*)defaultProfile;
@end


// Used to emulate an active running script, useful for testing purposes.
@interface FakeScriptCommand : NSScriptCommand {
  Method originalMethod_;
  Method alternateMethod_;
}
@end


// The base class for all our bookmark releated unit tests.
class BookmarkAppleScriptTest : public CocoaTest {
 public:
  BookmarkAppleScriptTest();
 private:
  BrowserTestHelper helper_;
  scoped_nsobject<FakeAppDelegate> appDelegate_;
 protected:
  scoped_nsobject<BookmarkFolderAppleScript> bookmarkBar_;
  BookmarkModel& model();
};

#endif
// CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_UNITTEST_H_
