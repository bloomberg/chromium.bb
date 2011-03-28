// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/debug/debugger.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/content_setting_bubble_model.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DummyContentSettingBubbleModel : public ContentSettingBubbleModel {
 public:
  DummyContentSettingBubbleModel(ContentSettingsType content_type)
      : ContentSettingBubbleModel(NULL, NULL, content_type) {
    RadioGroup radio_group;
    radio_group.default_item = 0;
    radio_group.radio_items.resize(2);
    set_radio_group(radio_group);
  }
};

class ContentSettingBubbleControllerTest : public CocoaTest {
};

// Check that the bubble doesn't crash or leak for any settings type
TEST_F(ContentSettingBubbleControllerTest, Init) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (i == CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
      continue;  // Notifications have no bubble.
    if (i == CONTENT_SETTINGS_TYPE_PRERENDER)
      continue;  // Prerender has no bubble.

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
        showForModel:new DummyContentSettingBubbleModel(settingsType)
        parentWindow:parent
         anchoredAt:NSMakePoint(50, 20)];
    EXPECT_TRUE(controller != nil);
    EXPECT_TRUE([[controller window] isVisible]);
    [parent.get() close];
  }
}

}  // namespace


