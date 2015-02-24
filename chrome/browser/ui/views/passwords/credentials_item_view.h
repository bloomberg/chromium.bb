// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "ui/views/controls/button/label_button.h"

namespace autofill {
struct PasswordForm;
}

namespace gfx {
class ImageSkia;
}

namespace net {
class URLRequestContextGetter;
}

namespace views {
class ImageView;
class Label;
}

// CredentialsItemView represents a credential view in the account chooser
// bubble.
class CredentialsItemView : public AccountAvatarFetcherDelegate,
                            public views::LabelButton {
 public:
  enum Style {
    ACCOUNT_CHOOSER,
    AUTO_SIGNIN,
  };

  CredentialsItemView(views::ButtonListener* button_listener,
                      const autofill::PasswordForm* form,
                      password_manager::CredentialType credential_type,
                      Style style,
                      net::URLRequestContextGetter* request_context);
  ~CredentialsItemView() override;

  const autofill::PasswordForm* form() const { return form_; }
  password_manager::CredentialType credential_type() const {
    return credential_type_;
  }

  // AccountAvatarFetcherDelegate:
  void UpdateAvatar(const gfx::ImageSkia& image) override;

 private:
  // views::LabelButton:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;

  const autofill::PasswordForm* form_;
  const password_manager::CredentialType credential_type_;

  views::ImageView* image_view_;
  views::Label* upper_label_;
  views::Label* lower_label_;

  base::WeakPtrFactory<CredentialsItemView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
