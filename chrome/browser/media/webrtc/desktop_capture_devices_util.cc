// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_devices_util.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

base::string16 GetStopSharingUIString(
    const base::string16& application_title,
    const base::string16& registered_extension_name,
    bool capture_audio,
    content::DesktopMediaID::Type capture_type) {
  if (!capture_audio) {
    if (application_title == registered_extension_name) {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT, application_title);
        case content::DesktopMediaID::TYPE_WINDOW:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_WINDOW_CAPTURE_NOTIFICATION_TEXT, application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_NOTIFICATION_TEXT, application_title);
        case content::DesktopMediaID::TYPE_NONE:
          NOTREACHED();
      }
    } else {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_WINDOW:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_WINDOW_CAPTURE_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_NONE:
          NOTREACHED();
      }
    }
  } else {  // The case with audio
    if (application_title == registered_extension_name) {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT,
              application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT,
              application_title);
        case content::DesktopMediaID::TYPE_NONE:
        case content::DesktopMediaID::TYPE_WINDOW:
          NOTREACHED();
      }
    } else {
      switch (capture_type) {
        case content::DesktopMediaID::TYPE_SCREEN:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_WEB_CONTENTS:
          return l10n_util::GetStringFUTF16(
              IDS_MEDIA_TAB_CAPTURE_WITH_AUDIO_NOTIFICATION_TEXT_DELEGATED,
              registered_extension_name, application_title);
        case content::DesktopMediaID::TYPE_NONE:
        case content::DesktopMediaID::TYPE_WINDOW:
          NOTREACHED();
      }
    }
  }
  return base::string16();
}

}  // namespace

std::unique_ptr<content::MediaStreamUI> GetDevicesForDesktopCapture(
    content::WebContents* web_contents,
    content::MediaStreamDevices* devices,
    content::DesktopMediaID media_id,
    content::MediaStreamType devices_video_type,
    content::MediaStreamType devices_audio_type,
    bool capture_audio,
    bool disable_local_echo,
    bool display_notification,
    const base::string16& application_title,
    const base::string16& registered_extension_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(2) << __func__ << ": media_id " << media_id.ToString()
           << ", capture_audio " << capture_audio << ", disable_local_echo "
           << disable_local_echo << ", display_notification "
           << display_notification << ", application_title "
           << application_title << ", extension_name "
           << registered_extension_name;

  // Add selected desktop source to the list.
  devices->push_back(content::MediaStreamDevice(
      devices_video_type, media_id.ToString(), media_id.ToString()));
  if (capture_audio) {
    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
      content::WebContentsMediaCaptureId web_id = media_id.web_contents_id;
      web_id.disable_local_echo = disable_local_echo;
      devices->push_back(content::MediaStreamDevice(
          devices_audio_type, web_id.ToString(), "Tab audio"));
    } else if (disable_local_echo) {
      // Use the special loopback device ID for system audio capture.
      devices->push_back(content::MediaStreamDevice(
          devices_audio_type,
          media::AudioDeviceDescription::kLoopbackWithMuteDeviceId,
          "System Audio"));
    } else {
      // Use the special loopback device ID for system audio capture.
      devices->push_back(content::MediaStreamDevice(
          devices_audio_type,
          media::AudioDeviceDescription::kLoopbackInputDeviceId,
          "System Audio"));
    }
  }

  // If required, register to display the notification for stream capture.
  std::unique_ptr<ScreenCaptureNotificationUI> notification_ui;
  if (display_notification) {
    notification_ui = ScreenCaptureNotificationUI::Create(
        GetStopSharingUIString(application_title, registered_extension_name,
                               capture_audio, media_id.type));
  }

  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RegisterMediaStream(web_contents, *devices, std::move(notification_ui));
}
