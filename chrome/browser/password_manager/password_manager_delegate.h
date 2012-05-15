// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_H_
#pragma once

namespace webkit {
namespace forms {
struct PasswordFormFillData;
}
}

class PasswordFormManager;
class Profile;

// An abstraction of operations in the external environment (WebContents)
// that the PasswordManager depends on.  This allows for more targeted
// unit testing.
class PasswordManagerDelegate {
 public:
  PasswordManagerDelegate() {}
  virtual ~PasswordManagerDelegate() {}

  // Fill forms matching |form_data| in |tab_contents|.  By default, goes
  // through the RenderViewHost to FillPasswordForm.  Tests can override this
  // to sever the dependency on the entire rendering stack.
  virtual void FillPasswordForm(
      const webkit::forms::PasswordFormFillData& form_data) = 0;

  // A mechanism to show an infobar in the current tab at our request.
  // The infobar may not show in some circumstances, such as when the one-click
  // sign in infobar is or will be shown.
  virtual void AddSavePasswordInfoBarIfPermitted(
      PasswordFormManager* form_to_save) = 0;

  // Get the profile for which we are managing passwords.
  virtual Profile* GetProfile() = 0;

  // If any SSL certificate errors were encountered as a result of the last
  // page load.
  virtual bool DidLastPageLoadEncounterSSLErrors() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDelegate);
};


#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DELEGATE_H_
