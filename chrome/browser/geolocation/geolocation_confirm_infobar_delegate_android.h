// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_ANDROID_H_

#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate.h"

class GeolocationConfirmInfoBarDelegateAndroid
    : public GeolocationConfirmInfoBarDelegate {
 public:
  GeolocationConfirmInfoBarDelegateAndroid(
      InfoBarTabHelper* infobar_helper,
      GeolocationInfoBarQueueController* controller,
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

 private:
  // ConfirmInfoBarDelegate:
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_ANDROID_H_
