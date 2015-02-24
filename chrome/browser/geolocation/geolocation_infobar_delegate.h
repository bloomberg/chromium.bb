// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/content_settings/permission_infobar_delegate.h"


// GeolocationInfoBarDelegates are created by the
// GeolocationInfoBarQueueController to control the display
// and handling of geolocation permission infobars to the user.
class GeolocationInfoBarDelegate :  public PermissionInfobarDelegate {
 public:
  // Creates a geolocation infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   PermissionQueueController* controller,
                                   const PermissionRequestID& id,
                                   const GURL& requesting_frame,
                                   const std::string& display_languages);

 private:
  GeolocationInfoBarDelegate(PermissionQueueController* controller,
                             const PermissionRequestID& id,
                             const GURL& requesting_frame,
                             int contents_unique_id,
                             const std::string& display_languages);
  ~GeolocationInfoBarDelegate() override;

  // PermissionInfoBarDelegate:
  int GetIconID() const override;
  base::string16 GetMessageText() const override;

  GURL requesting_frame_;
  std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_
