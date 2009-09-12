// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  BOOL changed_;
}
- (KeywordEditorModelObserver*)observer;
@end

@implementation FakeKeywordEditorController
- (void)modelChanged {
  changed_ = YES;
}
- (KeywordEditorModelObserver*)observer {
  return observer_.get();
}
@end

// TODO(rsesek): Figure out a good way to test this class (crbug.com/21640).

class KeywordEditorCocoaControllerTest : public PlatformTest {
 public:
  void SetUp() {
    TestingProfile* profile =
        static_cast<TestingProfile*>(browser_helper_.profile());
    profile->CreateTemplateURLModel();
    controller_.reset(
        [[FakeKeywordEditorController alloc] initWithProfile:profile]);
  }

  CocoaTestHelper cocoa_helper_;
  BrowserTestHelper browser_helper_;
  scoped_nsobject<FakeKeywordEditorController> controller_;
};

TEST_F(KeywordEditorCocoaControllerTest, TestModelChanged) {
  EXPECT_FALSE(controller_.get()->changed_);
  KeywordEditorModelObserver* observer = [controller_ observer];
  observer->OnTemplateURLModelChanged();
  EXPECT_TRUE(controller_.get()->changed_);
}
