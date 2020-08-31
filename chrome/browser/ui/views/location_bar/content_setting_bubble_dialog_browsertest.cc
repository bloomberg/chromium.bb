// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/tab_specific_content_settings_delegate.h"
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
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/notification_permission_ui_selector.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

using QuietUiReason =
    permissions::NotificationPermissionUiSelector::QuietUiReason;

// Test implementation of NotificationPermissionUiSelector that always forces
// the quiet UI to be used for surfacing notification permission requests.
class TestQuietNotificationPermissionUiSelector
    : public permissions::NotificationPermissionUiSelector {
 public:
  explicit TestQuietNotificationPermissionUiSelector(
      QuietUiReason simulated_reason_for_quiet_ui)
      : simulated_reason_for_quiet_ui_(simulated_reason_for_quiet_ui) {}
  ~TestQuietNotificationPermissionUiSelector() override = default;

 protected:
  // permissions::NotificationPermissionUiSelector:
  void SelectUiToUse(permissions::PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    std::move(callback).Run(
        Decision(simulated_reason_for_quiet_ui_, base::nullopt));
  }

 private:
  QuietUiReason simulated_reason_for_quiet_ui_;

  DISALLOW_COPY_AND_ASSIGN(TestQuietNotificationPermissionUiSelector);
};

}  // namespace

using ImageType = ContentSettingImageModel::ImageType;

class ContentSettingBubbleDialogTest : public DialogBrowserTest {
 public:
  ContentSettingBubbleDialogTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kQuietNotificationPrompts);
  }

  void ApplyMediastreamSettings(bool mic_accessed, bool camera_accessed);
  void ApplyContentSettingsForType(ContentSettingsType content_type);
  void TriggerQuietNotificationPermissionRequest(
      QuietUiReason simulated_reason_for_quiet_ui);

  void ShowDialogBubble(ContentSettingImageModel::ImageType image_type);

  void ShowUi(const std::string& name) override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::Optional<permissions::MockPermissionRequest>
      notification_permission_request_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingBubbleDialogTest);
};

void ContentSettingBubbleDialogTest::ApplyMediastreamSettings(
    bool mic_accessed,
    bool camera_accessed) {
  const int mic_setting =
      mic_accessed
          ? content_settings::TabSpecificContentSettings::MICROPHONE_ACCESSED
          : 0;
  const int camera_setting =
      camera_accessed
          ? content_settings::TabSpecificContentSettings::CAMERA_ACCESSED
          : 0;
  content_settings::TabSpecificContentSettings* content_settings =
      content_settings::TabSpecificContentSettings::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  content_settings->OnMediaStreamPermissionSet(
      GURL("https://example.com/"), mic_setting | camera_setting, std::string(),
      std::string(), std::string(), std::string());
}

void ContentSettingBubbleDialogTest::ApplyContentSettingsForType(
    ContentSettingsType content_type) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content_settings::TabSpecificContentSettings* content_settings =
      content_settings::TabSpecificContentSettings::FromWebContents(
          web_contents);
  switch (content_type) {
    case ContentSettingsType::AUTOMATIC_DOWNLOADS: {
      // Automatic downloads are handled by DownloadRequestLimiter.
      DownloadRequestLimiter::TabDownloadState* tab_download_state =
          g_browser_process->download_request_limiter()->GetDownloadState(
              web_contents, true);
      tab_download_state->set_download_seen();
      tab_download_state->SetDownloadStatusAndNotify(
          url::Origin::Create(web_contents->GetVisibleURL()),
          DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);
      break;
    }
    case ContentSettingsType::GEOLOCATION:
      content_settings->OnGeolocationPermissionSet(GURL("https://example.com/"),
                                                   false);
      break;
    case ContentSettingsType::MIDI_SYSEX:
      content_settings->OnMidiSysExAccessBlocked(GURL("https://example.com/"));
      break;
    case ContentSettingsType::POPUPS: {
      ui_test_utils::NavigateToURL(
          browser(),
          embedded_test_server()->GetURL("/popup_blocker/popup-many-10.html"));
      EXPECT_TRUE(content::ExecuteScript(web_contents, std::string()));
      auto* helper = PopupBlockerTabHelper::FromWebContents(web_contents);
      // popup-many-10.html should generate 10 blocked popups.
      EXPECT_EQ(10u, helper->GetBlockedPopupsCount());
      break;
    }
    case ContentSettingsType::PROTOCOL_HANDLERS:
      chrome::TabSpecificContentSettingsDelegate::FromWebContents(web_contents)
          ->set_pending_protocol_handler(ProtocolHandler::CreateProtocolHandler(
              "mailto", GURL("https://example.com/")));
      break;

    default:
      // For all other content_types passed in, mark them as blocked.
      content_settings->OnContentBlocked(content_type);
      break;
  }
  browser()->window()->UpdateToolbar(web_contents);
}

void ContentSettingBubbleDialogTest::TriggerQuietNotificationPermissionRequest(
    QuietUiReason simulated_reason_for_quiet_ui) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  permission_request_manager
      ->set_notification_permission_ui_selector_for_testing(
          std::make_unique<TestQuietNotificationPermissionUiSelector>(
              simulated_reason_for_quiet_ui));
  DCHECK(!notification_permission_request_);
  notification_permission_request_.emplace(
      "notifications",
      permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS,
      GURL("https://example.com"));
  permission_request_manager->AddRequest(&*notification_permission_request_);
  base::RunLoop().RunUntilIdle();
}

void ContentSettingBubbleDialogTest::ShowDialogBubble(
    ContentSettingImageModel::ImageType image_type) {
  LocationBarTesting* location_bar_testing =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();

  base::HistogramTester histograms;

  EXPECT_TRUE(location_bar_testing->TestContentSettingImagePressed(
      ContentSettingImageModel::GetContentSettingImageModelIndexForTesting(
          image_type)));

  histograms.ExpectBucketCount("ContentSettings.ImagePressed",
                               static_cast<int>(image_type), 1);
}

void ContentSettingBubbleDialogTest::ShowUi(const std::string& name) {
  if (base::StartsWith(name, "mediastream", base::CompareCase::SENSITIVE)) {
    ApplyMediastreamSettings(
        name == "mediastream_mic" || name == "mediastream_mic_and_camera",
        name == "mediastream_camera" || name == "mediastream_mic_and_camera");
    ShowDialogBubble(ImageType::MEDIASTREAM);
    return;
  }

  if (base::StartsWith(name, "notifications_quiet",
                       base::CompareCase::SENSITIVE)) {
    QuietUiReason reason = QuietUiReason::kEnabledInPrefs;
    if (name == "notifications_quiet_crowd_deny")
      reason = QuietUiReason::kTriggeredByCrowdDeny;
    else if (name == "notifications_quiet_abusive")
      reason = QuietUiReason::kTriggeredDueToAbusiveRequests;
    TriggerQuietNotificationPermissionRequest(reason);
    ShowDialogBubble(ImageType::NOTIFICATIONS_QUIET_PROMPT);
    return;
  }

  constexpr struct {
    const char* name;
    ContentSettingsType content_type;
    ContentSettingImageModel::ImageType image_type;
  } content_settings_values[] = {
      {"cookies", ContentSettingsType::COOKIES, ImageType::COOKIES},
      {"images", ContentSettingsType::IMAGES, ImageType::IMAGES},
      {"javascript", ContentSettingsType::JAVASCRIPT, ImageType::JAVASCRIPT},
      {"plugins", ContentSettingsType::PLUGINS, ImageType::PLUGINS},
      {"popups", ContentSettingsType::POPUPS, ImageType::POPUPS},
      {"geolocation", ContentSettingsType::GEOLOCATION, ImageType::GEOLOCATION},
      {"ppapi_broker", ContentSettingsType::PPAPI_BROKER,
       ImageType::PPAPI_BROKER},
      {"mixed_script", ContentSettingsType::MIXEDSCRIPT,
       ImageType::MIXEDSCRIPT},
      {"protocol_handlers", ContentSettingsType::PROTOCOL_HANDLERS,
       ImageType::PROTOCOL_HANDLERS},
      {"automatic_downloads", ContentSettingsType::AUTOMATIC_DOWNLOADS,
       ImageType::AUTOMATIC_DOWNLOADS},
      {"midi_sysex", ContentSettingsType::MIDI_SYSEX, ImageType::MIDI_SYSEX},
      {"ads", ContentSettingsType::ADS, ImageType::ADS},
  };
  for (auto content_settings : content_settings_values) {
    if (name == content_settings.name) {
      ApplyContentSettingsForType(content_settings.content_type);
      ShowDialogBubble(content_settings.image_type);
      return;
    }
  }
  ADD_FAILURE() << "Unknown dialog type";
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_cookies) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_images) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_javascript) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_plugins) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_popups) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_geolocation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_ppapi_broker) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_mixed_script) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_mediastream_mic) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_mediastream_camera) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_mediastream_mic_and_camera) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_protocol_handlers) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_automatic_downloads) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_midi_sysex) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest, InvokeUi_ads) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_notifications_quiet) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_notifications_quiet_crowd_deny) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleDialogTest,
                       InvokeUi_notifications_quiet_abusive) {
  ShowAndVerifyUi();
}
