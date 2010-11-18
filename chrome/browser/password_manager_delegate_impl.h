// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_DELEGATE_IMPL_H_

#include "base/basictypes.h"
#include "chrome/browser/password_manager/password_manager_delegate.h"

class TabContents;

class PasswordManagerDelegateImpl : public PasswordManagerDelegate {
 public:
  explicit PasswordManagerDelegateImpl(TabContents* contents)
      : tab_contents_(contents) { }

  // PasswordManagerDelegate implementation.
  virtual void FillPasswordForm(
      const webkit_glue::PasswordFormFillData& form_data);
  virtual void AddSavePasswordInfoBar(PasswordFormManager* form_to_save);
  virtual Profile* GetProfileForPasswordManager();
  virtual bool DidLastPageLoadEncounterSSLErrors();
 private:
  TabContents* tab_contents_;
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDelegateImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_DELEGATE_IMPL_H_
