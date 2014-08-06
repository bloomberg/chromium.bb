// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/content_settings/permission_request_id.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

class PermissionQueueController;
class InfoBarService;

// GeolocationInfoBarDelegates are created by the
// GeolocationInfoBarQueueController to control the display
// and handling of geolocation permission infobars to the user.
class GeolocationInfoBarDelegate : public ConfirmInfoBarDelegate {
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
  virtual ~GeolocationInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const NavigationDetails& details) const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;


  // Call back to the controller, to inform of the user's decision.
  void SetPermission(bool update_content_setting, bool allowed);

  // Marks a flag internally to indicate that the user has interacted with the
  // bar. This makes it possible to log from the destructor when the bar has not
  // been used, i.e. it has been ignored by the user.
  void set_user_has_interacted() {
    user_has_interacted_ = true;
  }

  PermissionQueueController* controller_;
  const PermissionRequestID id_;
  GURL requesting_frame_;
  int contents_unique_id_;
  std::string display_languages_;

  // Whether the user has interacted with the geolocation infobar.
  bool user_has_interacted_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_
