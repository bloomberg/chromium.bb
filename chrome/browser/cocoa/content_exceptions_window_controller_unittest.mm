// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/content_exceptions_window_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "base/ref_counted.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

void ProcessEvents() {
  for (;;) {
    base::mac::ScopedNSAutoreleasePool pool;
    NSEvent* next_event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                             untilDate:nil
                                                inMode:NSDefaultRunLoopMode
                                               dequeue:YES];
    if (!next_event)
      break;
    [NSApp sendEvent:next_event];
  }
}

void SendKeyEvents(NSString* characters) {
  for (NSUInteger i = 0; i < [characters length]; ++i) {
    unichar character = [characters characterAtIndex:i];
    NSString* charString = [NSString stringWithCharacters:&character length:1];
    NSEvent* event = [NSEvent keyEventWithType:NSKeyDown
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0.0
                                  windowNumber:0
                                       context:nil
                                    characters:charString
                   charactersIgnoringModifiers:charString
                                     isARepeat:NO
                                       keyCode:0];
    [NSApp sendEvent:event];
  }
}

class ContentExceptionsWindowControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    TestingProfile* profile = browser_helper_.profile();
    settingsMap_ = new HostContentSettingsMap(profile);
  }

  ContentExceptionsWindowController* GetController(ContentSettingsType type) {
    id controller = [ContentExceptionsWindowController
        controllerForType:type
              settingsMap:settingsMap_.get()
           otrSettingsMap:NULL];
    [controller showWindow:nil];
    return controller;
  }

  void ClickAdd(ContentExceptionsWindowController* controller) {
    [controller addException:nil];
    ProcessEvents();
  }

  void ClickRemove(ContentExceptionsWindowController* controller) {
    [controller removeException:nil];
    ProcessEvents();
  }

  void ClickRemoveAll(ContentExceptionsWindowController* controller) {
    [controller removeAllExceptions:nil];
    ProcessEvents();
  }

  void EnterText(NSString* str) {
    SendKeyEvents(str);
    ProcessEvents();
  }

  void HitEscape(ContentExceptionsWindowController* controller) {
    [controller cancel:nil];
    ProcessEvents();
  }

 protected:
  BrowserTestHelper browser_helper_;
  scoped_refptr<HostContentSettingsMap> settingsMap_;
};

TEST_F(ContentExceptionsWindowControllerTest, Construction) {
  ContentExceptionsWindowController* controller =
      [ContentExceptionsWindowController
          controllerForType:CONTENT_SETTINGS_TYPE_PLUGINS
                settingsMap:settingsMap_.get()
             otrSettingsMap:NULL];
  [controller showWindow:nil];
  [controller close];  // Should autorelease.
}

// Regression test for http://crbug.com/37137
TEST_F(ContentExceptionsWindowControllerTest, AddRemove) {
  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_PLUGINS);

  HostContentSettingsMap::SettingsForOneType settings;

  ClickAdd(controller);
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(0u, settings.size());

  ClickRemove(controller);

  EXPECT_FALSE([controller editingNewException]);
  [controller close];

  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(0u, settings.size());
}

// Regression test for http://crbug.com/37137
TEST_F(ContentExceptionsWindowControllerTest, AddRemoveAll) {
  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_PLUGINS);

  ClickAdd(controller);
  ClickRemoveAll(controller);

  EXPECT_FALSE([controller editingNewException]);
  [controller close];

  HostContentSettingsMap::SettingsForOneType settings;
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(0u, settings.size());
}

TEST_F(ContentExceptionsWindowControllerTest, Add) {
  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_PLUGINS);

  ClickAdd(controller);
  EnterText(@"addedhost\n");

  EXPECT_FALSE([controller editingNewException]);
  [controller close];

  HostContentSettingsMap::SettingsForOneType settings;
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(1u, settings.size());
  EXPECT_EQ(HostContentSettingsMap::Pattern("addedhost"), settings[0].first);
}

TEST_F(ContentExceptionsWindowControllerTest, AddEscDoesNotAdd) {
  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_PLUGINS);

  ClickAdd(controller);
  EnterText(@"addedhost");  // but do not press enter
  HitEscape(controller);

  HostContentSettingsMap::SettingsForOneType settings;
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(0u, settings.size());
  EXPECT_FALSE([controller editingNewException]);

  [controller close];
}

// Regression test for http://crbug.com/37208
TEST_F(ContentExceptionsWindowControllerTest, AddEditAddAdd) {
  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_PLUGINS);

  ClickAdd(controller);
  EnterText(@"testtesttest");  // but do not press enter
  ClickAdd(controller);
  ClickAdd(controller);

  EXPECT_TRUE([controller editingNewException]);
  [controller close];

  HostContentSettingsMap::SettingsForOneType settings;
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(0u, settings.size());
}

TEST_F(ContentExceptionsWindowControllerTest, AddExistingEditAdd) {
  settingsMap_->SetContentSetting(HostContentSettingsMap::Pattern("myhost"),
                                  CONTENT_SETTINGS_TYPE_PLUGINS,
                                  "",
                                  CONTENT_SETTING_BLOCK);

  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_PLUGINS);

  ClickAdd(controller);
  EnterText(@"myhost");  // but do not press enter
  ClickAdd(controller);

  EXPECT_TRUE([controller editingNewException]);
  [controller close];


  HostContentSettingsMap::SettingsForOneType settings;
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_PLUGINS,
                                      "",
                                      &settings);
  EXPECT_EQ(1u, settings.size());
}

TEST_F(ContentExceptionsWindowControllerTest, AddExistingDoesNotOverwrite) {
  settingsMap_->SetContentSetting(HostContentSettingsMap::Pattern("myhost"),
                                  CONTENT_SETTINGS_TYPE_COOKIES,
                                  "",
                                  CONTENT_SETTING_SESSION_ONLY);

  ContentExceptionsWindowController* controller =
      GetController(CONTENT_SETTINGS_TYPE_COOKIES);

  ClickAdd(controller);
  EnterText(@"myhost\n");

  EXPECT_FALSE([controller editingNewException]);
  [controller close];

  HostContentSettingsMap::SettingsForOneType settings;
  settingsMap_->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_COOKIES,
                                      "",
                                      &settings);
  EXPECT_EQ(1u, settings.size());
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY, settings[0].second);
}


}  // namespace
