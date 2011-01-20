// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/simple_content_exceptions_window_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/ref_counted.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_exceptions_table_model.h"
#include "chrome/browser/plugin_exceptions_table_model.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface SimpleContentExceptionsWindowController (Testing)

@property(readonly, nonatomic) TableModelArrayController* arrayController;

@end

@implementation SimpleContentExceptionsWindowController (Testing)

- (TableModelArrayController*)arrayController {
  return arrayController_;
}

@end


namespace {

class SimpleContentExceptionsWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    geolocation_settings_ = new GeolocationContentSettingsMap(profile);
    content_settings_ = new HostContentSettingsMap(profile);
  }

  SimpleContentExceptionsWindowController* GetController() {
    GeolocationExceptionsTableModel* model =  // Freed by window controller.
        new GeolocationExceptionsTableModel(geolocation_settings_.get());
    id controller = [SimpleContentExceptionsWindowController
        controllerWithTableModel:model];
    [controller showWindow:nil];
    return controller;
  }

  void ClickRemoveAll(SimpleContentExceptionsWindowController* controller) {
    [controller.arrayController removeAll:nil];
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_refptr<GeolocationContentSettingsMap> geolocation_settings_;
  scoped_refptr<HostContentSettingsMap> content_settings_;
};

TEST_F(SimpleContentExceptionsWindowControllerTest, Construction) {
  GeolocationExceptionsTableModel* model =  // Freed by window controller.
      new GeolocationExceptionsTableModel(geolocation_settings_.get());
  SimpleContentExceptionsWindowController* controller =
      [SimpleContentExceptionsWindowController controllerWithTableModel:model];
  [controller showWindow:nil];
  [controller close];  // Should autorelease.
}

TEST_F(SimpleContentExceptionsWindowControllerTest, ShowPluginExceptions) {
  PluginExceptionsTableModel* model =  // Freed by window controller.
      new PluginExceptionsTableModel(content_settings_.get(), NULL);
  SimpleContentExceptionsWindowController* controller =
      [SimpleContentExceptionsWindowController controllerWithTableModel:model];
  [controller showWindow:nil];
  [controller close];  // Should autorelease.
}

TEST_F(SimpleContentExceptionsWindowControllerTest, AddExistingEditAdd) {
  geolocation_settings_->SetContentSetting(
      GURL("http://myhost"), GURL(), CONTENT_SETTING_BLOCK);

  SimpleContentExceptionsWindowController* controller = GetController();
  ClickRemoveAll(controller);

  [controller close];

  EXPECT_EQ(0u, geolocation_settings_->GetAllOriginsSettings().size());
}

}  // namespace
