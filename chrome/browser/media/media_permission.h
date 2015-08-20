// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_PERMISSION_H_
#define CHROME_BROWSER_MEDIA_MEDIA_PERMISSION_H_

#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/media_stream_request.h"
#include "url/gurl.h"

class Profile;

// Represents a permission for microphone/camera access.
class MediaPermission {
 public:
  MediaPermission(ContentSettingsType content_type,
                  content::MediaStreamRequestType request_type,
                  const GURL& origin,
                  Profile* profile);

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
  ContentSetting GetStoredContentSetting() const;
  bool HasAvailableDevices(const std::string& device_id) const;

  const ContentSettingsType content_type_;
  const content::MediaStreamRequestType request_type_;
  const GURL origin_;
  const std::string device_id_;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(MediaPermission);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_PERMISSION_H_
