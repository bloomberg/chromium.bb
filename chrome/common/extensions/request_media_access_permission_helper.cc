// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/request_media_access_permission_helper.h"

#include "chrome/common/extensions/feature_switch.h"

namespace extensions {

// static
void RequestMediaAccessPermissionHelper::AuthorizeRequest(
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension,
    bool is_packaged_app) {
  content::MediaStreamDevices devices;

  bool accepted_an_audio_device = false;
  bool accepted_a_video_device = false;
  for (content::MediaStreamDeviceMap::const_iterator it =
       request->devices.begin(); it != request->devices.end(); ++it) {
    if (!accepted_an_audio_device &&
        content::IsAudioMediaType(it->first) &&
        !it->second.empty()) {
      // Require flag and tab capture permission for tab media.
      // Require audio capture permission for packaged apps.
      if ((it->first == content::MEDIA_TAB_AUDIO_CAPTURE &&
           extensions::FeatureSwitch::tab_capture()->IsEnabled() &&
           extension->HasAPIPermission(APIPermission::kTabCapture)) ||
          (it->first == content::MEDIA_DEVICE_AUDIO_CAPTURE &&
           is_packaged_app &&
           extension->HasAPIPermission(APIPermission::kAudioCapture))) {
        devices.push_back(it->second.front());
        accepted_an_audio_device = true;
      }
    } else if (!accepted_a_video_device &&
               content::IsVideoMediaType(it->first) &&
               !it->second.empty()) {
      // Require flag and tab capture permission for tab media.
      // Require video capture permission for packaged apps.
      if ((it->first == content::MEDIA_TAB_VIDEO_CAPTURE &&
           extensions::FeatureSwitch::tab_capture()->IsEnabled() &&
           extension->HasAPIPermission(APIPermission::kTabCapture)) ||
          (it->first == content::MEDIA_DEVICE_VIDEO_CAPTURE &&
           is_packaged_app &&
           extension->HasAPIPermission(APIPermission::kVideoCapture))) {
        devices.push_back(it->second.front());
        accepted_a_video_device = true;
      }
    }
  }

  callback.Run(devices);
}

}  // namespace extensions
