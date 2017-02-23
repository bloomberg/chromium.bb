// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_PERMISSION_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_PERMISSION_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/media_stream_request.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

// Represents a permission for microphone/camera access.
class MediaPermission {
 public:
  MediaPermission(ContentSettingsType content_type,
                  const GURL& requesting_origin,
                  const GURL& embedding_origin,
                  Profile* profile,
                  content::WebContents* web_contents);

  // Returns the status of the permission. If the setting is
  // CONTENT_SETTING_BLOCK, |denial_reason| will output the reason for it being
  // blocked.
  ContentSetting GetPermissionStatus(
      content::MediaStreamRequestResult* denial_reason) const;

  // Returns the status of the permission as with GetPermissionStatus but also
  // checks that the specified |device_id| is an available device.
  ContentSetting GetPermissionStatusWithDeviceRequired(
      const std::string& device_id,
      content::MediaStreamRequestResult* denial_reason) const;

 private:
  bool HasAvailableDevices(const std::string& device_id) const;

  const ContentSettingsType content_type_;
  const GURL requesting_origin_;
  const GURL embedding_origin_;
  const std::string device_id_;
  Profile* const profile_;
  content::WebContents* const web_contents_;

  DISALLOW_COPY_AND_ASSIGN(MediaPermission);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_PERMISSION_H_
