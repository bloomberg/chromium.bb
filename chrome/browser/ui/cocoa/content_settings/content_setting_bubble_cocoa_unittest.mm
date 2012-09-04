// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/debug/debugger.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

class DummyContentSettingBubbleModel : public ContentSettingBubbleModel {
 public:
  DummyContentSettingBubbleModel(TabContents* tab_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type)
      : ContentSettingBubbleModel(tab_contents, profile, content_type) {
    RadioGroup radio_group;
    radio_group.default_item = 0;
    radio_group.radio_items.resize(2);
    set_radio_group(radio_group);
  }
};

class ContentSettingBubbleControllerTest
    : public TabContentsTestHarness {
 public:
  ContentSettingBubbleControllerTest();
  virtual ~ContentSettingBubbleControllerTest();

 private:
  content::TestBrowserThread browser_thread_;

  base::mac::ScopedNSAutoreleasePool pool_;
};

ContentSettingBubbleControllerTest::ContentSettingBubbleControllerTest()
    : TabContentsTestHarness(),
      browser_thread_(BrowserThread::UI, &message_loop_) {
}

ContentSettingBubbleControllerTest::~ContentSettingBubbleControllerTest() {
}

// Check that the bubble doesn't crash or leak for any settings type
TEST_F(ContentSettingBubbleControllerTest, Init) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (i == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
        i == CONTENT_SETTINGS_TYPE_INTENTS ||
        i == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE ||
        i == CONTENT_SETTINGS_TYPE_FULLSCREEN ||
        i == CONTENT_SETTINGS_TYPE_MOUSELOCK ||
        i == CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
        i == CONTENT_SETTINGS_TYPE_PPAPI_BROKER) {
      // These types have no bubble.
      continue;
    }

    ContentSettingsType settingsType = static_cast<ContentSettingsType>(i);

    scoped_nsobject<NSWindow> parent([[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, 800, 600)
                  styleMask:NSBorderlessWindowMask
                    backing:NSBackingStoreBuffered
           defer:NO]);
    [parent setReleasedWhenClosed:NO];
    if (base::debug::BeingDebugged())
      [parent.get() orderFront:nil];
    else
      [parent.get() orderBack:nil];

    ContentSettingBubbleController* controller = [ContentSettingBubbleController
        showForModel:new DummyContentSettingBubbleModel(tab_contents(),
                                                        profile(),
                                                        settingsType)
        parentWindow:parent
          anchoredAt:NSMakePoint(50, 20)];
    EXPECT_TRUE(controller != nil);
    EXPECT_TRUE([[controller window] isVisible]);
    [parent.get() close];
  }
}

}  // namespace
