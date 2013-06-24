// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/base/cocoa/find_pasteboard.h"

// A subclass of FindPasteboard that doesn't write to the real find pasteboard.
@interface FindPasteboardTesting : FindPasteboard {
 @public
  int notificationCount_;
 @private
  NSPasteboard* pboard_;
}
- (NSPasteboard*)findPboard;

- (void)callback:(id)sender;

// These are for checking that pasteboard content is copied to/from the
// FindPasteboard correctly.
- (NSString*)findPboardText;
- (void)setFindPboardText:(NSString*)text;
@end

@implementation FindPasteboardTesting

- (id)init {
  if ((self = [super init])) {
    pboard_ = [NSPasteboard pasteboardWithUniqueName];
  }
  return self;
}

- (void)dealloc {
  [pboard_ releaseGlobally];
  [super dealloc];
}

- (NSPasteboard*)findPboard {
  return pboard_;
}

- (void)callback:(id)sender {
  ++notificationCount_;
}

- (void)setFindPboardText:(NSString*)text {
  [pboard_ declareTypes:[NSArray arrayWithObject:NSStringPboardType]
                  owner:nil];
  [pboard_ setString:text forType:NSStringPboardType];
}

- (NSString*)findPboardText {
  return [pboard_ stringForType:NSStringPboardType];
}
@end

namespace {

class FindPasteboardTest : public CocoaTest {
 public:
  FindPasteboardTest() {
    pboard_.reset([[FindPasteboardTesting alloc] init]);
  }
 protected:
  base::scoped_nsobject<FindPasteboardTesting> pboard_;
};

TEST_F(FindPasteboardTest, SettingTextUpdatesPboard) {
  [pboard_.get() setFindText:@"text"];
  EXPECT_EQ(
      NSOrderedSame,
      [[pboard_.get() findPboardText] compare:@"text"]);
}

TEST_F(FindPasteboardTest, ReadingFromPboardUpdatesFindText) {
  [pboard_.get() setFindPboardText:@"text"];
  [pboard_.get() loadTextFromPasteboard:nil];
  EXPECT_EQ(
      NSOrderedSame,
      [[pboard_.get() findText] compare:@"text"]);
}

TEST_F(FindPasteboardTest, SendsNotificationWhenTextChanges) {
  [[NSNotificationCenter defaultCenter]
      addObserver:pboard_.get()
         selector:@selector(callback:)
             name:kFindPasteboardChangedNotification
           object:pboard_.get()];
  EXPECT_EQ(0, pboard_.get()->notificationCount_);
  [pboard_.get() setFindText:@"text"];
  EXPECT_EQ(1, pboard_.get()->notificationCount_);
  [pboard_.get() setFindText:@"text"];
  EXPECT_EQ(1, pboard_.get()->notificationCount_);
  [pboard_.get() setFindText:@"other text"];
  EXPECT_EQ(2, pboard_.get()->notificationCount_);

  [pboard_.get() setFindPboardText:@"other text"];
  [pboard_.get() loadTextFromPasteboard:nil];
  EXPECT_EQ(2, pboard_.get()->notificationCount_);

  [pboard_.get() setFindPboardText:@"otherer text"];
  [pboard_.get() loadTextFromPasteboard:nil];
  EXPECT_EQ(3, pboard_.get()->notificationCount_);

  [[NSNotificationCenter defaultCenter] removeObserver:pboard_.get()];
}


}  // namespace
