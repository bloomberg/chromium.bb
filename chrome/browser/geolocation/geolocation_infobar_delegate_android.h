// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"

class GoogleLocationSettingsHelper;

class GeolocationInfoBarDelegateAndroid : public GeolocationInfoBarDelegate {
 public:
  GeolocationInfoBarDelegateAndroid(
      PermissionQueueController* controller,
      const PermissionRequestID& id,
      const GURL& requesting_frame_url,
      int contents_unique_id,
      const std::string& display_languages,
      const std::string& accept_button_label);

 private:
  virtual ~GeolocationInfoBarDelegateAndroid();

  // ConfirmInfoBarDelegate:
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  scoped_ptr<GoogleLocationSettingsHelper> google_location_settings_helper_;

  std::string accept_button_label_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_
