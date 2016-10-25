// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"

// GeolocationInfoBarDelegateAndroids are created by the
// PermissionQueueController to control the display
// and handling of geolocation permission infobars to the user.
class GeolocationInfoBarDelegateAndroid : public PermissionInfoBarDelegate {
 public:
  GeolocationInfoBarDelegateAndroid(const GURL& requesting_frame,
                                    bool user_gesture,
                                    Profile* profile,
                                    const PermissionSetCallback& callback);

 private:
  ~GeolocationInfoBarDelegateAndroid() override;

  // PermissionInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  int GetMessageResourceId() const override;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_ANDROID_H_
