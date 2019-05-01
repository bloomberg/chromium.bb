// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_manager_infobar_delegate.h"

@protocol ApplicationCommands;

namespace password_manager {
class PasswordFormManagerForUI;
}

// After a successful *new* login attempt, Chrome passes the current
// password_manager::PasswordFormManager and move it to a
// IOSChromeSavePasswordInfoBarDelegate while the user makes up their mind
// with the "save password" infobar.
// If |password_update| is true the delegate will use "Update" related strings,
// and should Update the credentials instead of Saving new ones.
class IOSChromeSavePasswordInfoBarDelegate
    : public IOSChromePasswordManagerInfoBarDelegate {
 public:
  IOSChromeSavePasswordInfoBarDelegate(
      bool is_sync_user,
      bool password_update,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);

  ~IOSChromeSavePasswordInfoBarDelegate() override;

  // InfoBarDelegate implementation
  bool ShouldExpire(const NavigationDetails& details) const override;

  // ConfirmInfoBarDelegate implementation.
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // Updates the credentials being saved with |username| and |password|.
  void UpdateCredentials(NSString* username, NSString* password);

  // true if password is being updated at the moment the InfobarModal is
  // created.
  bool IsPasswordUpdate() const;

  // The title for the InfobarModal being presented.
  NSString* GetInfobarModalTitleText() const;

 private:
  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

  // true if password is being updated at the moment the InfobarModal is
  // created.
  bool password_update_ = false;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSavePasswordInfoBarDelegate);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
