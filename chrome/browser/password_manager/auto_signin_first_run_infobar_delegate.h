// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_AUTO_SIGNIN_FIRST_RUN_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_AUTO_SIGNIN_FIRST_RUN_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/password_manager/password_manager_infobar_delegate.h"

namespace content {
class WebContents;
}

// This delegate provides and acts on data for the android only auto sign-in
// first run experience infobar.
class AutoSigninFirstRunInfoBarDelegate
    : public PasswordManagerInfoBarDelegate {
 public:
  static void Create(content::WebContents* web_contents,
                     const base::string16& message);

  ~AutoSigninFirstRunInfoBarDelegate() override;

  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  base::string16 GetExplanation() { return explanation_; }

 protected:
  // Makes a ctor available in tests.
  AutoSigninFirstRunInfoBarDelegate(content::WebContents* web_contents,
                                    bool is_smart_lock_branding_available,
                                    const base::string16& message);

 private:
  base::string16 explanation_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AutoSigninFirstRunInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_AUTO_SIGNIN_FIRST_RUN_INFOBAR_DELEGATE_H_
