// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_settings_dialog_controller.h"

#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ContentSettingsDialogControllerTest : public CocoaTest {
 public:
  BrowserTestHelper browser_helper_;
};

// Test that +showContentSettingsDialogForProfile brings up the existing editor
// and doesn't leak or crash.
TEST_F(ContentSettingsDialogControllerTest, CreateDialog) {
  Profile* profile(browser_helper_.profile());
  ContentSettingsDialogController* sharedInstance =
      [ContentSettingsDialogController
          showContentSettingsForType:CONTENT_SETTINGS_TYPE_DEFAULT
                             profile:profile];
  EXPECT_TRUE(sharedInstance);
  [sharedInstance close];
}

}  // namespace
