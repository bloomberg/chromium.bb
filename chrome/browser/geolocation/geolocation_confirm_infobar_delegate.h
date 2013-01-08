// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_H_

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/geolocation/geolocation_permission_request_id.h"
#include "googleurl/src/gurl.h"

#include <string>

class GeolocationInfoBarQueueController;
class InfoBarService;

// GeolocationInfoBarDelegates are created by the
// GeolocationInfoBarQueueController to control the display
// and handling of geolocation permission infobars to the user.
class GeolocationConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a geolocation delegate and adds it to |infobar_service|.  Returns
  // the delegate if it was successfully added.
  static InfoBarDelegate* Create(InfoBarService* infobar_service,
                                 GeolocationInfoBarQueueController* controller,
                                 const GeolocationPermissionRequestID& id,
                                 const GURL& requesting_frame,
                                 const std::string& display_languages);

 protected:
  GeolocationConfirmInfoBarDelegate(
      InfoBarService* infobar_service,
      GeolocationInfoBarQueueController* controller,
      const GeolocationPermissionRequestID& id,
      const GURL& requesting_frame,
      const std::string& display_languages);

  const GeolocationPermissionRequestID& id() const { return id_; }

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // Call back to the controller, to inform of the user's decision.
  void SetPermission(bool update_content_setting, bool allowed);

 private:
  GeolocationInfoBarQueueController* controller_;
  const GeolocationPermissionRequestID id_;
  GURL requesting_frame_;
  int contents_unique_id_;
  std::string display_languages_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GeolocationConfirmInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_H_
