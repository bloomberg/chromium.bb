// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/password_manager/content_password_manager_driver.h"
#include "chrome/browser/password_manager/password_manager_delegate.h"
#include "content/public/browser/web_contents_user_data.h"

class PasswordGenerationManager;
class PasswordManager;

namespace content {
class WebContents;
}

class PasswordManagerDelegateImpl
    : public PasswordManagerDelegate,
      public content::WebContentsUserData<PasswordManagerDelegateImpl> {
 public:
  virtual ~PasswordManagerDelegateImpl();

  // PasswordManagerDelegate implementation.
  virtual void AddSavePasswordInfoBarIfPermitted(
      PasswordFormManager* form_to_save) OVERRIDE;
  virtual Profile* GetProfile() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PasswordManagerDriver* GetDriver() OVERRIDE;

  // Convenience method to allow //chrome code easy access to a PasswordManager
  // from a WebContents instance.
  static PasswordManager* GetManagerFromWebContents(
      content::WebContents* contents);

  // Convenience method to allow //chrome code easy access to a
  // PasswordGenerationManager
  // from a WebContents instance.
  static PasswordGenerationManager* GetGenerationManagerFromWebContents(
      content::WebContents* contents);

 private:
  explicit PasswordManagerDelegateImpl(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PasswordManagerDelegateImpl>;

  content::WebContents* web_contents_;
  ContentPasswordManagerDriver driver_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDelegateImpl);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_IMPL_H_
