// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"

using content::WebContents;

typedef InProcessBrowserTest ContentSettingImageModelBrowserTest;

// Tests that every model creates a valid bubble.
IN_PROC_BROWSER_TEST_F(ContentSettingImageModelBrowserTest, CreateBubbleModel) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  content_settings->BlockAllContentForTesting();

  // Automatic downloads are handled by DownloadRequestLimiter.
  g_browser_process->download_request_limiter()
      ->GetDownloadState(web_contents, web_contents, true)
      ->SetDownloadStatusAndNotify(
          DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);

  // Test that image models tied to a single content setting create bubbles tied
  // to the same setting.
  static const ContentSettingsType content_settings_to_test[] = {
      CONTENT_SETTINGS_TYPE_COOKIES,
      CONTENT_SETTINGS_TYPE_IMAGES,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      CONTENT_SETTINGS_TYPE_POPUPS,
      CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
      CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS,
      CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
  };

  Profile* profile = browser()->profile();
  for (ContentSettingsType type : content_settings_to_test) {
    std::unique_ptr<ContentSettingBubbleModel> bubble(
        ContentSettingSimpleImageModel::CreateForContentTypeForTesting(type)
            ->CreateBubbleModel(nullptr, web_contents, profile));

    // All of the above content settings should create a
    // ContentSettingSimpleBubbleModel that is tied to a particular setting,
    // and thus it should be an instance of ContentSettingSimpleBubbleModel.
    ContentSettingSimpleBubbleModel* simple_bubble =
        bubble->AsSimpleBubbleModel();
    ASSERT_TRUE(simple_bubble);
    EXPECT_EQ(type, simple_bubble->content_type());
  }

  // For other models, we can only test that they create a valid bubble.
  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    EXPECT_TRUE(base::WrapUnique(
                    model->CreateBubbleModel(nullptr, web_contents, profile))
                    .get());
  }
}

// Tests that we correctly remember for which WebContents the animation has run,
// and thus we should not run it again.
IN_PROC_BROWSER_TEST_F(ContentSettingImageModelBrowserTest,
                       ShouldRunAnimation) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::unique_ptr<ContentSettingImageModel> model =
      ContentSettingSimpleImageModel::CreateForContentTypeForTesting(
          CONTENT_SETTINGS_TYPE_IMAGES);

  EXPECT_TRUE(model->ShouldRunAnimation(web_contents));
  model->SetAnimationHasRun(web_contents);
  EXPECT_FALSE(model->ShouldRunAnimation(web_contents));

  // The animation has run for the current WebContents, but not for any other.
  Profile* profile = browser()->profile();
  WebContents::CreateParams create_params(profile);
  WebContents* other_web_contents = WebContents::Create(create_params);
  browser()->tab_strip_model()->TabStripModel::AppendWebContents(
      other_web_contents, true);
  EXPECT_TRUE(model->ShouldRunAnimation(other_web_contents));
}
