// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_exceptions_window_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/ref_counted.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ContentExceptionsWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    settingsMap_ = new HostContentSettingsMap(profile);
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_refptr<HostContentSettingsMap> settingsMap_;
};

TEST_F(ContentExceptionsWindowControllerTest, Construction) {
  ContentExceptionsWindowController* controller =
      [ContentExceptionsWindowController
          showForType:CONTENT_SETTINGS_TYPE_PLUGINS
          settingsMap:settingsMap_.get()];
  [controller close];  // Should autorelease.
}

}  // namespace
