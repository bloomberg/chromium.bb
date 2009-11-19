// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/keyword_editor_cocoa_controller.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface FakeKeywordEditorController : KeywordEditorCocoaController {
 @public
  BOOL modelChanged_;
}
- (void)modelChanged;
- (BOOL)hasModelChanged;
- (KeywordEditorModelObserver*)observer;
@end

@implementation FakeKeywordEditorController

- (void)modelChanged {
  modelChanged_ = YES;
}

- (BOOL)hasModelChanged {
  return modelChanged_;
}

- (KeywordEditorModelObserver*)observer {
  return observer_.get();
}

@end

// TODO(rsesek): Figure out a good way to test this class (crbug.com/21640).

namespace {

class KeywordEditorCocoaControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile =
        static_cast<TestingProfile*>(browser_helper_.profile());
    profile->CreateTemplateURLModel();

    controller_ = [[FakeKeywordEditorController alloc] initWithProfile:profile];
  }

  virtual void TearDown() {
    [controller_ close];
    CocoaTest::TearDown();
  }

  // Helper to count the keyword editors.
  NSUInteger CountKeywordEditors() {
    base::ScopedNSAutoreleasePool pool;
    NSUInteger count = 0;
    for (NSWindow* window in [NSApp windows]) {
      id controller = [window windowController];
      if ([controller isKindOfClass:[KeywordEditorCocoaController class]]) {
        ++count;
      }
    }
    return count;
  }

  BrowserTestHelper browser_helper_;
  FakeKeywordEditorController* controller_;
};

TEST_F(KeywordEditorCocoaControllerTest, TestModelChanged) {
  EXPECT_FALSE([controller_ hasModelChanged]);
  KeywordEditorModelObserver* observer = [controller_ observer];
  observer->OnTemplateURLModelChanged();
  EXPECT_TRUE([controller_ hasModelChanged]);
}

// Test that +showKeywordEditor brings up the existing editor and
// creates one if needed.
TEST_F(KeywordEditorCocoaControllerTest, ShowKeywordEditor) {
  // No outstanding editors.
  Profile* profile(browser_helper_.profile());
  KeywordEditorCocoaController* sharedInstance =
      [KeywordEditorCocoaController sharedInstanceForProfile:profile];
  EXPECT_TRUE(nil == sharedInstance);
  EXPECT_EQ(CountKeywordEditors(), 0U);

  const NSUInteger initial_window_count([[NSApp windows] count]);

  // The window unwinds using -autorelease, so we need to introduce an
  // autorelease pool to really test whether it went away or not.
  {
    base::ScopedNSAutoreleasePool pool;

    // +showKeywordEditor: creates a new controller.
    [KeywordEditorCocoaController showKeywordEditor:profile];
    sharedInstance =
        [KeywordEditorCocoaController sharedInstanceForProfile:profile];
    EXPECT_TRUE(sharedInstance);
    EXPECT_EQ(CountKeywordEditors(), 1U);

    // Another call doesn't create another controller.
    [KeywordEditorCocoaController showKeywordEditor:profile];
    EXPECT_TRUE(sharedInstance ==
        [KeywordEditorCocoaController sharedInstanceForProfile:profile]);
    EXPECT_EQ(CountKeywordEditors(), 1U);

    [sharedInstance close];
  }

  // No outstanding editors.
  sharedInstance =
      [KeywordEditorCocoaController sharedInstanceForProfile:profile];
  EXPECT_TRUE(nil == sharedInstance);
  EXPECT_EQ(CountKeywordEditors(), 0U);

  // Windows we created should be gone.
  EXPECT_EQ([[NSApp windows] count], initial_window_count);

  // Get a new editor, should be different from the previous one.
  [KeywordEditorCocoaController showKeywordEditor:profile];
  KeywordEditorCocoaController* newSharedInstance =
      [KeywordEditorCocoaController sharedInstanceForProfile:profile];
  EXPECT_TRUE(sharedInstance != newSharedInstance);
  EXPECT_EQ(CountKeywordEditors(), 1U);
  [newSharedInstance close];
}

}  // namespace
