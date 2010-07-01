// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/geolocation_exceptions_window_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/ref_counted.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class GeolocationExceptionsWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    settingsMap_ = new GeolocationContentSettingsMap(profile);
  }

  GeolocationExceptionsWindowController* GetController() {
    id controller = [GeolocationExceptionsWindowController
        controllerWithSettingsMap:settingsMap_.get()];
    [controller showWindow:nil];
    return controller;
  }

  void ClickRemoveAll(GeolocationExceptionsWindowController* controller) {
    [controller removeAll:nil];
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_refptr<GeolocationContentSettingsMap> settingsMap_;
};

TEST_F(GeolocationExceptionsWindowControllerTest, Construction) {
  GeolocationExceptionsWindowController* controller =
      [GeolocationExceptionsWindowController
          controllerWithSettingsMap:settingsMap_.get()];
  [controller showWindow:nil];
  [controller close];  // Should autorelease.
}

TEST_F(GeolocationExceptionsWindowControllerTest, AddExistingEditAdd) {
  settingsMap_->SetContentSetting(
      GURL("http://myhost"), GURL(), CONTENT_SETTING_BLOCK);

  GeolocationExceptionsWindowController* controller = GetController();
  ClickRemoveAll(controller);

  [controller close];

  EXPECT_EQ(0u, settingsMap_->GetAllOriginsSettings().size());
}

}  // namespace
