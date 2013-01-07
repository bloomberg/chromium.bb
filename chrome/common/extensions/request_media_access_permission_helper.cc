// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/request_media_access_permission_helper.h"

#include "chrome/common/extensions/feature_switch.h"

namespace extensions {

// static
void RequestMediaAccessPermissionHelper::AuthorizeRequest(
    const content::MediaStreamDevices& devices,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension,
    bool is_packaged_app) {
  content::MediaStreamDevices accepted_devices;
  bool accepted_an_audio_device = false;
  bool accepted_a_video_device = false;
  for (content::MediaStreamDevices::const_iterator it =
       devices.begin(); it != devices.end(); ++it) {
    if (!accepted_an_audio_device && content::IsAudioMediaType(it->type)) {
      // Require flag and tab capture permission for tab media.
      // Require audio capture permission for packaged apps.
      if ((request.audio_type == content::MEDIA_TAB_AUDIO_CAPTURE &&
           extensions::FeatureSwitch::tab_capture()->IsEnabled() &&
           extension->HasAPIPermission(APIPermission::kTabCapture)) ||
          (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE &&
           is_packaged_app &&
           extension->HasAPIPermission(APIPermission::kAudioCapture))) {
        accepted_devices.push_back(*it);
        accepted_an_audio_device = true;
      }
    } else if (!accepted_a_video_device &&
               content::IsVideoMediaType(it->type)) {
      // Require flag and tab capture permission for tab media.
      // Require video capture permission for packaged apps.
      if ((request.video_type == content::MEDIA_TAB_VIDEO_CAPTURE &&
           extensions::FeatureSwitch::tab_capture()->IsEnabled() &&
           extension->HasAPIPermission(APIPermission::kTabCapture)) ||
          (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE &&
           is_packaged_app &&
           extension->HasAPIPermission(APIPermission::kVideoCapture))) {
        accepted_devices.push_back(*it);
        accepted_a_video_device = true;
      }
    }
  }

  callback.Run(accepted_devices);
}

}  // namespace extensions
