// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_UNITTEST_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_UNITTEST_H_

#import <Cocoa/Cocoa.h>
#import <objc/objc-runtime.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/ui/cocoa/applescript/bookmark_folder_applescript.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "testing/platform_test.h"

class BookmarkModel;

// The fake object that acts as our app's delegate, useful for testing purposes.
@interface FakeAppDelegate : AppController {
 @public
  CocoaProfileTest* test_;  // weak.
}
@property(nonatomic) CocoaProfileTest* test;
// Return the |TestingProfile*| which is used for testing.
- (Profile*)lastProfile;
@end


// Used to emulate an active running script, useful for testing purposes.
@interface FakeScriptCommand : NSScriptCommand {
  Method originalMethod_;
  Method alternateMethod_;
}
@end


// The base class for all our bookmark releated unit tests.
class BookmarkAppleScriptTest : public CocoaProfileTest {
 public:
  BookmarkAppleScriptTest();
  virtual ~BookmarkAppleScriptTest();
  virtual void SetUp() OVERRIDE;
 private:
  base::scoped_nsobject<FakeAppDelegate> appDelegate_;

 protected:
  base::scoped_nsobject<BookmarkFolderAppleScript> bookmarkBar_;
};

#endif  // CHROME_BROWSER_UI_COCOA_APPLESCRIPT_BOOKMARK_APPLESCRIPT_UTILS_UNITTEST_H_
