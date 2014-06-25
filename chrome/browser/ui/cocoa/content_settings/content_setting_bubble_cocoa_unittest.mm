// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/media_stream_request.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class DummyContentSettingBubbleModel : public ContentSettingBubbleModel {
 public:
  DummyContentSettingBubbleModel(content::WebContents* web_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type)
      : ContentSettingBubbleModel(web_contents, profile, content_type) {
    RadioGroup radio_group;
    radio_group.default_item = 0;
    radio_group.radio_items.resize(2);
    set_radio_group(radio_group);
    MediaMenu micMenu;
    micMenu.label = "Microphone:";
    add_media_menu(content::MEDIA_DEVICE_AUDIO_CAPTURE, micMenu);
    MediaMenu cameraMenu;
    cameraMenu.label = "Camera:";
    add_media_menu(content::MEDIA_DEVICE_VIDEO_CAPTURE, cameraMenu);
  }
};

class ContentSettingBubbleControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  // Helper function to create the bubble controller.
  ContentSettingBubbleController* CreateBubbleController(
      ContentSettingsType settingsType);

  virtual void SetUp() OVERRIDE {
    ChromeUnitTestSuite::InitializeProviders();
    ChromeUnitTestSuite::InitializeResourceBundle();
    content_client_.reset(new ChromeContentClient);
    content::SetContentClient(content_client_.get());
    browser_content_client_.reset(new chrome::ChromeContentBrowserClient());
    content::SetBrowserClientForTesting(browser_content_client_.get());
    initializer_.reset(new TestingBrowserProcessInitializer);
    ChromeRenderViewHostTestHarness::SetUp();
  }

  scoped_ptr<ChromeContentClient> content_client_;
  scoped_ptr<chrome::ChromeContentBrowserClient> browser_content_client_;

  // This is a unit test running in the browser_tests suite, so we must create
  // the TestingBrowserProcess manually. Must be first member.
  scoped_ptr<TestingBrowserProcessInitializer> initializer_;

  base::scoped_nsobject<NSWindow> parent_;

 private:
  base::mac::ScopedNSAutoreleasePool pool_;
};

ContentSettingBubbleController*
ContentSettingBubbleControllerTest::CreateBubbleController(
    ContentSettingsType settingsType) {
  parent_.reset([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600)
                                            styleMask:NSBorderlessWindowMask
                                              backing:NSBackingStoreBuffered
                                                defer:NO]);
  [parent_ setReleasedWhenClosed:NO];
  [parent_ orderFront:nil];

  ContentSettingBubbleController* controller = [ContentSettingBubbleController
      showForModel:new DummyContentSettingBubbleModel(web_contents(),
                                                      profile(),
                                                      settingsType)
       webContents:web_contents()
      parentWindow:parent_
        anchoredAt:NSMakePoint(50, 20)];

  EXPECT_TRUE(controller);
  EXPECT_TRUE([[controller window] isVisible]);

  return controller;
}

// Check that the bubble doesn't crash or leak for any settings type
TEST_F(ContentSettingBubbleControllerTest, Init) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    if (i == CONTENT_SETTINGS_TYPE_NOTIFICATIONS ||
        i == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE ||
        i == CONTENT_SETTINGS_TYPE_FULLSCREEN ||
        i == CONTENT_SETTINGS_TYPE_MOUSELOCK ||
        i == CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
        i == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
        i == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA ||
        i == CONTENT_SETTINGS_TYPE_PPAPI_BROKER ||
        i == CONTENT_SETTINGS_TYPE_MIDI_SYSEX ||
        i == CONTENT_SETTINGS_TYPE_PUSH_MESSAGING) {
      // These types have no bubble.
      continue;
    }

    ContentSettingsType settingsType = static_cast<ContentSettingsType>(i);

    ContentSettingBubbleController* controller =
        CreateBubbleController(settingsType);
    EXPECT_EQ(0u, [controller mediaMenus]->size());
    [parent_ close];
  }
}

// Check that the bubble works for CONTENT_SETTINGS_TYPE_MEDIASTREAM.
TEST_F(ContentSettingBubbleControllerTest, MediaStreamBubble) {
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();
  ContentSettingBubbleController* controller =
      CreateBubbleController(CONTENT_SETTINGS_TYPE_MEDIASTREAM);
  content_setting_bubble::MediaMenuPartsMap* mediaMenus =
      [controller mediaMenus];
  EXPECT_EQ(2u, mediaMenus->size());
  NSString* title = l10n_util::GetNSString(IDS_MEDIA_MENU_NO_DEVICE_TITLE);
  for (content_setting_bubble::MediaMenuPartsMap::const_iterator i =
       mediaMenus->begin(); i != mediaMenus->end(); ++i) {
    EXPECT_TRUE((content::MEDIA_DEVICE_AUDIO_CAPTURE == i->second->type) ||
                (content::MEDIA_DEVICE_VIDEO_CAPTURE == i->second->type));
    EXPECT_EQ(0, [i->first numberOfItems]);
    EXPECT_NSEQ(title, [i->first title]);
    EXPECT_FALSE([i->first isEnabled]);
  }

 [parent_ close];
}

}  // namespace
