// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class ContentSettingBubbleDialogTest : public DialogBrowserTest {
 public:
  ContentSettingBubbleDialogTest() {}

  void ShowDialogBubble(ContentSettingsType content_type);

  void ShowDialog(const std::string& name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingBubbleDialogTest);
};

void ContentSettingBubbleDialogTest::ShowDialogBubble(
    ContentSettingsType content_type) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  switch (content_type) {
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      content_settings->OnMediaStreamPermissionSet(
          GURL::EmptyGURL(),
          content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
              ? TabSpecificContentSettings::MICROPHONE_ACCESSED
              : TabSpecificContentSettings::CAMERA_ACCESSED,
          std::string(), std::string(), std::string(), std::string());
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      content_settings->OnGeolocationPermissionSet(GURL::EmptyGURL(), false);
      break;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      // Automatic downloads are handled by DownloadRequestLimiter.
      g_browser_process->download_request_limiter()
          ->GetDownloadState(web_contents, web_contents, true)
          ->SetDownloadStatusAndNotify(
              DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS: {
      GURL url(
          embedded_test_server()->GetURL("/popup_blocker/popup-many-10.html"));
      ui_test_utils::NavigateToURL(browser(), url);
      EXPECT_TRUE(content::ExecuteScript(web_contents, std::string()));
      auto* helper = PopupBlockerTabHelper::FromWebContents(web_contents);
      // popup-many-10.html should generate 10 blocked popups.
      EXPECT_EQ(10u, helper->GetBlockedPopupsCount());
      break;
    }
    case CONTENT_SETTINGS_TYPE_PLUGINS: {
      const base::string16 plugin_name = base::ASCIIToUTF16("plugin_name");
      content_settings->OnContentBlockedWithDetail(content_type, plugin_name);
      break;
    }

    default:
      // For all other content_types passed in, mark them as blocked.
      content_settings->OnContentBlocked(content_type);
      break;
  }
  browser()->window()->UpdateToolbar(web_contents);
  LocationBarTesting* location_bar_testing =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  EXPECT_TRUE(location_bar_testing->TestContentSettingImagePressed(
      ContentSettingImageModel::GetContentSettingImageModelIndexForTesting(
          content_type)));
}

void ContentSettingBubbleDialogTest::ShowDialog(const std::string& name) {
  constexpr struct {
    const char* name;
    ContentSettingsType content_type;
  } content_settings_values[] = {
      {"cookies", CONTENT_SETTINGS_TYPE_COOKIES},
      {"images", CONTENT_SETTINGS_TYPE_IMAGES},
      {"javascript", CONTENT_SETTINGS_TYPE_JAVASCRIPT},
      {"plugins", CONTENT_SETTINGS_TYPE_PLUGINS},
      {"popups", CONTENT_SETTINGS_TYPE_POPUPS},
      {"geolocation", CONTENT_SETTINGS_TYPE_GEOLOCATION},
      {"ppapi_broker", CONTENT_SETTINGS_TYPE_PPAPI_BROKER},
      {"mixed_script", CONTENT_SETTINGS_TYPE_MIXEDSCRIPT},
      {"mediastream_mic", CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC},
      {"mediastream_camera", CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA},
      {"protocol_handlers", CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS},
      {"automatic_downloads", CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS},
      {"midi_sysex", CONTENT_SETTINGS_TYPE_MIDI_SYSEX},
      {"ads", CONTENT_SETTINGS_TYPE_ADS}};
  for (auto content_settings : content_settings_values) {
    if (name == content_settings.name) {
      ShowDialogBubble(content_settings.content_type);
      return;
    }
  }
  ADD_FAILURE() << "Unknown dialog type";
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeDialog_cookies) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeDialog_images) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_javascript) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeDialog_plugins) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeDialog_popups) {
  ASSERT_TRUE(embedded_test_server()->Start());
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_geolocation) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_ppapi_broker) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_mixed_script) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_mediastream_mic) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_mediastream_camera) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_protocol_handlers) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_automatic_downloads) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeDialog_midi_sysex) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeDialog_ads) {
  RunDialog();
}
