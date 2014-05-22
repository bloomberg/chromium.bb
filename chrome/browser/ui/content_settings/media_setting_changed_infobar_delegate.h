// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_SETTING_CHANGED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_SETTING_CHANGED_INFOBAR_DELEGATE_H_

#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// An infobar that tells the user a reload is required after changing media
// settings, and allows a reload via a button on the infobar.
class MediaSettingChangedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a media setting changed infobar and delegate and adds the infobar
  // to |infobar_service|.
  static void Create(InfoBarService* infobar_service);

 private:
  MediaSettingChangedInfoBarDelegate();
  virtual ~MediaSettingChangedInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MediaSettingChangedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_MEDIA_SETTING_CHANGED_INFOBAR_DELEGATE_H_
