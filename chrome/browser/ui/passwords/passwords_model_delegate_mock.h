// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class PasswordsModelDelegateMock
    : public PasswordsModelDelegate,
      public base::SupportsWeakPtr<PasswordsModelDelegateMock>{
 public:
  PasswordsModelDelegateMock();
  ~PasswordsModelDelegateMock() override;

  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(GetOrigin, const GURL&());
  MOCK_CONST_METHOD0(GetState, password_manager::ui::State());
  MOCK_CONST_METHOD0(GetPendingPassword, const autofill::PasswordForm&());
  MOCK_CONST_METHOD0(IsPasswordOverridden, bool());
  MOCK_CONST_METHOD0(
      GetCurrentForms,
      const std::vector<std::unique_ptr<autofill::PasswordForm>>&());
  MOCK_CONST_METHOD0(GetCurrentInteractionStats,
                     password_manager::InteractionsStats*());
  MOCK_METHOD0(OnBubbleShown, void());
  MOCK_METHOD0(OnBubbleHidden, void());
  MOCK_METHOD0(OnNoInteractionOnUpdate, void());
  MOCK_METHOD0(OnNopeUpdateClicked, void());
  MOCK_METHOD0(NeverSavePassword, void());
  MOCK_METHOD0(SavePassword, void());
  MOCK_METHOD1(UpdatePassword, void(const autofill::PasswordForm&));
  MOCK_METHOD2(ChooseCredential, void(const autofill::PasswordForm&,
                                      password_manager::CredentialType));
  MOCK_METHOD0(NavigateToExternalPasswordManager, void());
  MOCK_METHOD0(NavigateToSmartLockPage, void());
  MOCK_METHOD0(NavigateToSmartLockHelpPage, void());
  MOCK_METHOD0(NavigateToPasswordManagerSettingsPage, void());
  MOCK_METHOD0(NavigateToChromeSignIn, void());
  MOCK_METHOD0(OnDialogHidden, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsModelDelegateMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_MOCK_H_
