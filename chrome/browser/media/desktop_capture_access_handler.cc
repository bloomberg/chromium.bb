// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/desktop_capture_access_handler.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/desktop_streams_registry.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/screen_capture_notification_ui.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/origin_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "media/audio/audio_device_description.h"
#include "net/base/url_util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "base/sha1.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

namespace {

bool IsExtensionWhitelistedForScreenCapture(
    const extensions::Extension* extension) {
  if (!extension)
    return false;

#if defined(OS_CHROMEOS)
  std::string hash = base::SHA1HashString(extension->id());
  std::string hex_hash = base::HexEncode(hash.c_str(), hash.length());

  // crbug.com/446688
  return hex_hash == "4F25792AF1AA7483936DE29C07806F203C7170A0" ||
         hex_hash == "BD8781D757D830FC2E85470A1B6E8A718B7EE0D9" ||
         hex_hash == "4AC2B6C63C6480D150DFDA13E4A5956EB1D0DDBB" ||
         hex_hash == "81986D4F846CEDDDB962643FA501D1780DD441BB";
#else
  return false;
#endif  // defined(OS_CHROMEOS)
}

bool IsBuiltInExtension(const GURL& origin) {
  return
      // Feedback Extension.
      origin.spec() == "chrome-extension://gfdkimpbcpahaombhbimeihdjnejgicl/";
}

// Helper to get title of the calling application shown in the screen capture
// notification.
base::string16 GetApplicationTitle(content::WebContents* web_contents,
                                   const extensions::Extension* extension) {
  // Use extension name as title for extensions and host/origin for drive-by
  // web.
  std::string title;
  if (extension) {
    title = extension->name();
    return base::UTF8ToUTF16(title);
  }
  GURL url = web_contents->GetURL();
  title = content::IsOriginSecure(url) ? net::GetHostAndOptionalPort(url)
                                       : url.GetOrigin().spec();
  return base::UTF8ToUTF16(title);
}

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
// Helper to get list of media stream devices for desktop capture in |devices|.
// Registers to display notification if |display_notification| is true.
// Returns an instance of MediaStreamUI to be passed to content layer.
std::unique_ptr<content::MediaStreamUI> GetDevicesForDesktopCapture(
    content::MediaStreamDevices* devices,
    content::DesktopMediaID media_id,
    bool capture_audio,
    bool display_notification,
    const base::string16& application_title,
    const base::string16& registered_extension_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<content::MediaStreamUI> ui;

  // Add selected desktop source to the list.
  devices->push_back(content::MediaStreamDevice(
      content::MEDIA_DESKTOP_VIDEO_CAPTURE, media_id.ToString(), "Screen"));
  if (capture_audio) {
    if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
      devices->push_back(
          content::MediaStreamDevice(content::MEDIA_DESKTOP_AUDIO_CAPTURE,
                                     media_id.ToString(), "Tab audio"));
    } else {
      // Use the special loopback device ID for system audio capture.
      devices->push_back(content::MediaStreamDevice(
          content::MEDIA_DESKTOP_AUDIO_CAPTURE,
          media::AudioDeviceDescription::kLoopbackInputDeviceId,
          "System Audio"));
    }
  }

  // If required, register to display the notification for stream capture.
  if (!display_notification) {
    return ui;
  }

  ui = ScreenCaptureNotificationUI::Create(GetStopSharingUIString(
      application_title, registered_extension_name, capture_audio,
      media_id.type));

  return ui;
}

#if !defined(OS_ANDROID)
// Find browser or app window from a given |web_contents|.
gfx::NativeWindow FindParentWindowForWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && browser->window())
    return browser->window()->GetNativeWindow();

  const extensions::AppWindowRegistry::AppWindowList& window_list =
      extensions::AppWindowRegistry::Get(web_contents->GetBrowserContext())
          ->app_windows();
  for (extensions::AppWindowRegistry::AppWindowList::const_iterator iter =
           window_list.begin();
       iter != window_list.end(); ++iter) {
    if ((*iter)->web_contents() == web_contents)
      return (*iter)->GetNativeWindow();
  }

  return NULL;
}
#endif

}  // namespace

DesktopCaptureAccessHandler::DesktopCaptureAccessHandler() {
}

DesktopCaptureAccessHandler::~DesktopCaptureAccessHandler() {
}

void DesktopCaptureAccessHandler::ProcessScreenCaptureAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  DCHECK_EQ(request.video_type, content::MEDIA_DESKTOP_VIDEO_CAPTURE);

  bool loopback_audio_supported = false;
#if defined(USE_CRAS) || defined(OS_WIN)
  // Currently loopback audio capture is supported only on Windows and ChromeOS.
  loopback_audio_supported = true;
#endif

  bool component_extension = false;
  component_extension =
      extension && extension->location() == extensions::Manifest::COMPONENT;

  bool screen_capture_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUserMediaScreenCapturing) ||
      MediaCaptureDevicesDispatcher::IsOriginForCasting(
          request.security_origin) ||
      IsExtensionWhitelistedForScreenCapture(extension) ||
      IsBuiltInExtension(request.security_origin);

  const bool origin_is_secure =
      content::IsOriginSecure(request.security_origin) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowHttpScreenCapture);

  // If basic conditions (screen capturing is enabled and origin is secure)
  // aren't fulfilled, we'll use "invalid state" as result. Otherwise, we set
  // it after checking permission.
  // TODO(grunell): It would be good to change this result for something else,
  // probably a new one.
  content::MediaStreamRequestResult result =
      content::MEDIA_DEVICE_INVALID_STATE;

  // Approve request only when the following conditions are met:
  //  1. Screen capturing is enabled via command line switch or white-listed for
  //     the given origin.
  //  2. Request comes from a page with a secure origin or from an extension.
  if (screen_capture_enabled && origin_is_secure) {
    // Get title of the calling application prior to showing the message box.
    // chrome::ShowQuestionMessageBox() starts a nested message loop which may
    // allow |web_contents| to be destroyed on the UI thread before the messag
    // box is closed. See http://crbug.com/326690.
    base::string16 application_title =
        GetApplicationTitle(web_contents, extension);
#if !defined(OS_ANDROID)
    gfx::NativeWindow parent_window =
        FindParentWindowForWebContents(web_contents);
#else
    gfx::NativeWindow parent_window = NULL;
#endif
    web_contents = NULL;

    bool whitelisted_extension =
        IsExtensionWhitelistedForScreenCapture(extension);

    // For whitelisted or component extensions, bypass message box.
    bool user_approved = false;
    if (!whitelisted_extension && !component_extension) {
      base::string16 application_name =
          base::UTF8ToUTF16(request.security_origin.spec());
      if (extension)
        application_name = base::UTF8ToUTF16(extension->name());
      base::string16 confirmation_text = l10n_util::GetStringFUTF16(
          request.audio_type == content::MEDIA_NO_SERVICE
              ? IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TEXT
              : IDS_MEDIA_SCREEN_AND_AUDIO_CAPTURE_CONFIRMATION_TEXT,
          application_name);
      chrome::MessageBoxResult result = chrome::ShowQuestionMessageBox(
          parent_window,
          l10n_util::GetStringFUTF16(
              IDS_MEDIA_SCREEN_CAPTURE_CONFIRMATION_TITLE, application_name),
          confirmation_text);
      user_approved = (result == chrome::MESSAGE_BOX_RESULT_YES);
    }

    if (user_approved || component_extension || whitelisted_extension) {
      content::DesktopMediaID screen_id;
#if defined(OS_CHROMEOS)
      screen_id = content::DesktopMediaID::RegisterAuraWindow(
          content::DesktopMediaID::TYPE_SCREEN,
          ash::Shell::GetInstance()->GetPrimaryRootWindow());
#else   // defined(OS_CHROMEOS)
      screen_id = content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                          webrtc::kFullDesktopScreenId);
#endif  // !defined(OS_CHROMEOS)

      bool capture_audio =
          (request.audio_type == content::MEDIA_DESKTOP_AUDIO_CAPTURE &&
           loopback_audio_supported);

      // Unless we're being invoked from a component extension, register to
      // display the notification for stream capture.
      bool display_notification = !component_extension;

      ui = GetDevicesForDesktopCapture(&devices, screen_id, capture_audio,
                                       display_notification, application_title,
                                       application_title);
      DCHECK(!devices.empty());
    }

    // The only case when devices can be empty is if the user has denied
    // permission.
    result = devices.empty() ? content::MEDIA_DEVICE_PERMISSION_DENIED
                             : content::MEDIA_DEVICE_OK;
  }

  callback.Run(devices, result, std::move(ui));
}

bool DesktopCaptureAccessHandler::SupportsStreamType(
    const content::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == content::MEDIA_DESKTOP_VIDEO_CAPTURE ||
         type == content::MEDIA_DESKTOP_AUDIO_CAPTURE;
}

bool DesktopCaptureAccessHandler::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void DesktopCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  if (request.video_type != content::MEDIA_DESKTOP_VIDEO_CAPTURE) {
    callback.Run(devices, content::MEDIA_DEVICE_INVALID_STATE, std::move(ui));
    return;
  }

  // If the device id wasn't specified then this is a screen capture request
  // (i.e. chooseDesktopMedia() API wasn't used to generate device id).
  if (request.requested_video_device_id.empty()) {
    ProcessScreenCaptureAccessRequest(web_contents, request, callback,
                                      extension);
    return;
  }

  // The extension name that the stream is registered with.
  std::string original_extension_name;
  // Resolve DesktopMediaID for the specified device id.
  content::DesktopMediaID media_id;
  // TODO(miu): Replace "main RenderFrame" IDs with the request's actual
  // RenderFrame IDs once the desktop capture extension API implementation is
  // fixed.  http://crbug.com/304341
  content::WebContents* const web_contents_for_stream =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(request.render_process_id,
                                           request.render_frame_id));
  content::RenderFrameHost* const main_frame =
      web_contents_for_stream ? web_contents_for_stream->GetMainFrame() : NULL;
  if (main_frame) {
    media_id = MediaCaptureDevicesDispatcher::GetInstance()
                   ->GetDesktopStreamsRegistry()
                   ->RequestMediaForStreamId(request.requested_video_device_id,
                                             main_frame->GetProcess()->GetID(),
                                             main_frame->GetRoutingID(),
                                             request.security_origin,
                                             &original_extension_name);
  }

  // Received invalid device id.
  if (media_id.type == content::DesktopMediaID::TYPE_NONE) {
    callback.Run(devices, content::MEDIA_DEVICE_INVALID_STATE, std::move(ui));
    return;
  }

  bool loopback_audio_supported = false;
#if defined(USE_CRAS) || defined(OS_WIN)
  // Currently loopback audio capture is supported only on Windows and ChromeOS.
  loopback_audio_supported = true;
#endif

  // This value essentially from the checkbox on picker window, so it
  // corresponds to user permission.
  const bool audio_permitted = media_id.audio_share;

  // This value essentially from whether getUserMedia requests audio stream.
  const bool audio_requested =
      request.audio_type == content::MEDIA_DESKTOP_AUDIO_CAPTURE;

  // This value shows for a given capture type, whether the system or our code
  // can support audio sharing. Currently audio is only supported for screen and
  // tab/webcontents capture streams.
  const bool audio_supported =
      (media_id.type == content::DesktopMediaID::TYPE_SCREEN &&
       loopback_audio_supported) ||
      media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS;

  const bool check_audio_permission =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          extensions::switches::kDisableDesktopCaptureAudio);
  const bool capture_audio =
      (check_audio_permission ? audio_permitted : true) && audio_requested &&
      audio_supported;

  ui = GetDevicesForDesktopCapture(&devices, media_id, capture_audio, true,
                                   GetApplicationTitle(web_contents, extension),
                                   base::UTF8ToUTF16(original_extension_name));

  callback.Run(devices, content::MEDIA_DEVICE_OK, std::move(ui));
}

void DesktopCaptureAccessHandler::UpdateMediaRequestState(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    content::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Track desktop capture sessions.  Tracking is necessary to avoid unbalanced
  // session counts since not all requests will reach MEDIA_REQUEST_STATE_DONE,
  // but they will all reach MEDIA_REQUEST_STATE_CLOSING.
  if (stream_type != content::MEDIA_DESKTOP_VIDEO_CAPTURE)
    return;

  if (state == content::MEDIA_REQUEST_STATE_DONE) {
    DesktopCaptureSession session = {
        render_process_id, render_frame_id, page_request_id};
    desktop_capture_sessions_.push_back(session);
  } else if (state == content::MEDIA_REQUEST_STATE_CLOSING) {
    for (DesktopCaptureSessions::iterator it =
             desktop_capture_sessions_.begin();
         it != desktop_capture_sessions_.end(); ++it) {
      if (it->render_process_id == render_process_id &&
          it->render_frame_id == render_frame_id &&
          it->page_request_id == page_request_id) {
        desktop_capture_sessions_.erase(it);
        break;
      }
    }
  }
}

bool DesktopCaptureAccessHandler::IsCaptureInProgress() {
  return desktop_capture_sessions_.size() > 0;
}
