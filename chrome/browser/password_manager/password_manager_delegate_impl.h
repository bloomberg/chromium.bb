// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/password_manager/password_manager_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class PasswordManagerDelegateImpl
    : public PasswordManagerDelegate,
      public content::WebContentsUserData<PasswordManagerDelegateImpl> {
 public:
  virtual ~PasswordManagerDelegateImpl();

  // PasswordManagerDelegate implementation.
  virtual void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) OVERRIDE;
  virtual void AddSavePasswordInfoBarIfPermitted(
      PasswordFormManager* form_to_save) OVERRIDE;
  virtual Profile* GetProfile() OVERRIDE;
  virtual bool DidLastPageLoadEncounterSSLErrors() OVERRIDE;

 private:
  explicit PasswordManagerDelegateImpl(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PasswordManagerDelegateImpl>;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDelegateImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_IMPL_H_
