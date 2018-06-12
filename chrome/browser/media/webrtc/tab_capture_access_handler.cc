// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_capture_access_handler.h"

#include <utility>

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/permissions/permissions_data.h"

TabCaptureAccessHandler::TabCaptureAccessHandler() {
}

TabCaptureAccessHandler::~TabCaptureAccessHandler() {
}

bool TabCaptureAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const content::MediaStreamType type,
    const extensions::Extension* extension) {
  return type == content::MEDIA_TAB_VIDEO_CAPTURE ||
         type == content::MEDIA_TAB_AUDIO_CAPTURE;
}

bool TabCaptureAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return false;
}

void TabCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  content::MediaStreamDevices devices;
  std::unique_ptr<content::MediaStreamUI> ui;

  if (!extension) {
    std::move(callback).Run(devices, content::MEDIA_DEVICE_TAB_CAPTURE_FAILURE,
                            std::move(ui));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  extensions::TabCaptureRegistry* tab_capture_registry =
      extensions::TabCaptureRegistry::Get(profile);
  if (!tab_capture_registry) {
    NOTREACHED();
    std::move(callback).Run(devices, content::MEDIA_DEVICE_INVALID_STATE,
                            std::move(ui));
    return;
  }
  const bool tab_capture_allowed = tab_capture_registry->VerifyRequest(
      request.render_process_id, request.render_frame_id, extension->id());

  if (request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE &&
      tab_capture_allowed &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kTabCapture)) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_TAB_AUDIO_CAPTURE, std::string(), std::string()));
  }

  if (request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE &&
      tab_capture_allowed &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kTabCapture)) {
    devices.push_back(content::MediaStreamDevice(
        content::MEDIA_TAB_VIDEO_CAPTURE, std::string(), std::string()));
  }

  if (!devices.empty()) {
    ui = MediaCaptureDevicesDispatcher::GetInstance()
             ->GetMediaStreamCaptureIndicator()
             ->RegisterMediaStream(web_contents, devices);
  }
  UpdateExtensionTrusted(request, extension);
  std::move(callback).Run(devices,
                          devices.empty() ? content::MEDIA_DEVICE_INVALID_STATE
                                          : content::MEDIA_DEVICE_OK,
                          std::move(ui));
}
