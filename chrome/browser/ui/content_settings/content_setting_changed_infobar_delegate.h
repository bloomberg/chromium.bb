// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_CHANGED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_CHANGED_INFOBAR_DELEGATE_H_

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"

class InfoBarService;

// This class configures an infobar shown when a content setting dialog is
// closed and a setting that requires reload to take affect has been changed.
// The user is shown a message indicating that a reload of the page is required,
// and presented a button to perform the reload right from the infobar. It takes
// an icon and message text as arguments.
class ContentSettingChangedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  virtual ~ContentSettingChangedInfoBarDelegate();

  // Creates a collected cookies delegate and adds it to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     int icon,
                     int message_text);

#if defined(UNIT_TEST)
  static scoped_ptr<ContentSettingChangedInfoBarDelegate> CreateForUnitTest(
      InfoBarService* infobar_service,
      int icon,
      int message_text) {
    return scoped_ptr<ContentSettingChangedInfoBarDelegate>(
        new ContentSettingChangedInfoBarDelegate(infobar_service,
                                                 icon,
                                                 message_text));
  }
#endif

 private:
  ContentSettingChangedInfoBarDelegate(InfoBarService* infobar_service,
                                       int icon,
                                       int message_text);

  // ConfirmInfoBarDelegate overrides.
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  int icon_;
  int message_text_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingChangedInfoBarDelegate);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_CHANGED_INFOBAR_DELEGATE_H_
