// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_

// The flow for geolocation permissions on Android needs to take into account
// the global geolocation settings so it differs from the desktop one. It
// works as follows.
// GeolocationPermissionContextAndroid::RequestPermission intercepts the flow
// and proceeds to check the system location.
// This will in fact check several possible settings
//     - The global system geolocation setting
//     - The Google location settings on pre KK devices
//     - An old internal Chrome setting on pre-JB MR1 devices
// With all that information it will decide if system location is enabled.
// If enabled, it proceeds with the per site flow via
// GeolocationPermissionContext (which will check per site permissions, create
// infobars, etc.).
//
// Otherwise the permission is already decided.
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"

namespace content {
class WebContents;
}

namespace infobars {
class InfoBar;
}

class LocationSettings;
class GURL;
class PermissionRequestID;

class GeolocationPermissionContextAndroid
    : public GeolocationPermissionContext {
 public:
  explicit GeolocationPermissionContextAndroid(Profile* profile);
  ~GeolocationPermissionContextAndroid() override;

 protected:
  // GeolocationPermissionContext:
  ContentSetting GetPermissionStatusInternal(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
  friend class GeolocationPermissionContextTests;

  // GeolocationPermissionContext:
  void RequestPermission(
      content::WebContents* web_contents,
      const PermissionRequestID& id,
      const GURL& requesting_frame_origin,
      bool user_gesture,
      const BrowserPermissionCallback& callback) override;
  void CancelPermissionRequest(content::WebContents* web_contents,
                               const PermissionRequestID& id) override;
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           const BrowserPermissionCallback& callback,
                           bool persist,
                           ContentSetting content_setting) override;

  void HandleUpdateAndroidPermissions(const PermissionRequestID& id,
                                      const GURL& requesting_frame_origin,
                                      const GURL& embedding_origin,
                                      const BrowserPermissionCallback& callback,
                                      bool permissions_updated);

  // Overrides the LocationSettings object used to determine whether
  // system and Chrome-wide location permissions are enabled.
  void SetLocationSettingsForTesting(
      std::unique_ptr<LocationSettings> settings);

  std::unique_ptr<LocationSettings> location_settings_;

  // This is owned by the InfoBarService (owner of the InfoBar).
  infobars::InfoBar* permission_update_infobar_;

  // Must be the last member, to ensure that it will be destroyed first, which
  // will invalidate weak pointers.
  base::WeakPtrFactory<GeolocationPermissionContextAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_ANDROID_H_
