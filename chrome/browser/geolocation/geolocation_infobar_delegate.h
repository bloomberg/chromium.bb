// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_

#include <string>

#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "url/gurl.h"

class PermissionQueueController;
class InfoBarService;

// GeolocationInfoBarDelegates are created by the
// GeolocationInfoBarQueueController to control the display
// and handling of geolocation permission infobars to the user.
class GeolocationInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a geolocation infobar delegate and adds it to |infobar_service|.
  // Returns the delegate if it was successfully added.
  static InfoBarDelegate* Create(InfoBarService* infobar_service,
                                 PermissionQueueController* controller,
                                 const PermissionRequestID& id,
                                 const GURL& requesting_frame,
                                 const std::string& display_languages);

 protected:
  GeolocationInfoBarDelegate(InfoBarService* infobar_service,
                             PermissionQueueController* controller,
                             const PermissionRequestID& id,
                             const GURL& requesting_frame,
                             int contents_unique_id,
                             const std::string& display_languages);
  virtual ~GeolocationInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual bool Accept() OVERRIDE;

  // Call back to the controller, to inform of the user's decision.
  void SetPermission(bool update_content_setting, bool allowed);

 private:
  // ConfirmInfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  PermissionQueueController* controller_;
  const PermissionRequestID id_;
  GURL requesting_frame_;
  int contents_unique_id_;
  std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_DELEGATE_H_
