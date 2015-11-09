// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
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
      CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS,
      CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
  };

  Profile* profile = browser()->profile();
  for (ContentSettingsType type : content_settings_to_test) {
    scoped_ptr<ContentSettingBubbleModel> bubble(
        ContentSettingSimpleImageModel::CreateForContentTypeForTesting(type)
            ->CreateBubbleModel(nullptr, web_contents, profile));
    EXPECT_EQ(type, bubble->content_type());
  }

  // For other models, we can only test that they create a valid bubble.
  ScopedVector<ContentSettingImageModel> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (ContentSettingImageModel* model : models) {
    EXPECT_TRUE(make_scoped_ptr(
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

  scoped_ptr<ContentSettingImageModel> model =
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
