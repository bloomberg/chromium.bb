// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/content_settings/content_setting_bubble_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/media_stream_request.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

class ContentSettingBubbleControllerTest : public InProcessBrowserTest {
 protected:
  ContentSettingBubbleControllerTest() {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() {
    return browser()->profile();
  }

  // Helper function to create the bubble controller.
  ContentSettingBubbleController* CreateBubbleController(
      ContentSettingBubbleModel* bubble);

  base::scoped_nsobject<NSWindow> parent_;

 private:
  base::mac::ScopedNSAutoreleasePool pool_;
};

ContentSettingBubbleController*
ContentSettingBubbleControllerTest::CreateBubbleController(
    ContentSettingBubbleModel* bubble) {
  parent_.reset([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 800, 600)
                                            styleMask:NSBorderlessWindowMask
                                              backing:NSBackingStoreBuffered
                                                defer:NO]);
  [parent_ setReleasedWhenClosed:NO];
  [parent_ orderFront:nil];

  ContentSettingBubbleController* controller =
      [ContentSettingBubbleController showForModel:bubble
                                       webContents:web_contents()
                                      parentWindow:parent_
                                        decoration:nullptr
                                        anchoredAt:NSMakePoint(50, 20)];

  EXPECT_TRUE(controller);
  EXPECT_TRUE([[controller window] isVisible]);

  return controller;
}

// Check that the bubble doesn't crash or leak for any image model.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleControllerTest, Init) {
  TabSpecificContentSettings::FromWebContents(web_contents())->
      BlockAllContentForTesting();

  // Automatic downloads are handled by DownloadRequestLimiter.
  g_browser_process->download_request_limiter()
      ->GetDownloadState(web_contents(), web_contents(), true)
      ->SetDownloadStatusAndNotify(
          DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (const auto& model : models) {
    ContentSettingBubbleModel* bubble =
        model->CreateBubbleModel(nullptr, web_contents(), profile());
    ContentSettingBubbleController* controller = CreateBubbleController(bubble);
    // No bubble except the one for media should have media menus.
    if (!bubble->AsMediaStreamBubbleModel())
      EXPECT_EQ(0u, [controller mediaMenus]->size());
    [parent_ close];
  }
}

// The media bubble is specific in that in serves two content setting types.
// Test that it works too.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleControllerTest, MediaStreamBubble) {
  TabSpecificContentSettings::FromWebContents(web_contents())->
      BlockAllContentForTesting();
  MediaCaptureDevicesDispatcher::GetInstance()->
      DisableDeviceEnumerationForTesting();
  ContentSettingBubbleController* controller = CreateBubbleController(
      new ContentSettingMediaStreamBubbleModel(
          nullptr, web_contents(), profile()));
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
