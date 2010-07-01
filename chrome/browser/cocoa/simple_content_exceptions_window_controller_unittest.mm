// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/simple_content_exceptions_window_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/ref_counted.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class SimpleContentExceptionsWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    settingsMap_ = new GeolocationContentSettingsMap(profile);
  }

  SimpleContentExceptionsWindowController* GetController() {
  GeolocationExceptionsTableModel* model =  // Freed by window controller.
      new GeolocationExceptionsTableModel(settingsMap_.get());
    id controller = [SimpleContentExceptionsWindowController
        controllerWithTableModel:model];
    [controller showWindow:nil];
    return controller;
  }

  void ClickRemoveAll(SimpleContentExceptionsWindowController* controller) {
    [controller removeAll:nil];
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_refptr<GeolocationContentSettingsMap> settingsMap_;
};

TEST_F(SimpleContentExceptionsWindowControllerTest, Construction) {
  GeolocationExceptionsTableModel* model =  // Freed by window controller.
      new GeolocationExceptionsTableModel(settingsMap_.get());
  SimpleContentExceptionsWindowController* controller =
      [SimpleContentExceptionsWindowController controllerWithTableModel:model];
  [controller showWindow:nil];
  [controller close];  // Should autorelease.
}

TEST_F(SimpleContentExceptionsWindowControllerTest, AddExistingEditAdd) {
  settingsMap_->SetContentSetting(
      GURL("http://myhost"), GURL(), CONTENT_SETTING_BLOCK);

  SimpleContentExceptionsWindowController* controller = GetController();
  ClickRemoveAll(controller);

  [controller close];

  EXPECT_EQ(0u, settingsMap_->GetAllOriginsSettings().size());
}

}  // namespace
