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
      InfoBarService* infobar_service,
      GeolocationInfoBarQueueController* controller,
      const GeolocationPermissionRequestID& id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

 private:
  virtual ~GeolocationInfoBarDelegateAndroid();

  // ConfirmInfoBarDelegate:
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  scoped_ptr<GoogleLocationSettingsHelper> google_location_settings_helper_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_
