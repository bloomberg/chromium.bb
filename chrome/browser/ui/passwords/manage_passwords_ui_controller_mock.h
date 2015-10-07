// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/navigation_details.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
enum class CredentialType;
}

// This mock is used in tests to ensure that we're just testing the controller
// behavior, and not the behavior of the bits and pieces it relies upon (like
// FormManager).
class ManagePasswordsUIControllerMock
    : public ManagePasswordsUIController {
 public:
  explicit ManagePasswordsUIControllerMock(
      content::WebContents* contents);
  ~ManagePasswordsUIControllerMock() override;

  // Navigation, surprisingly, is platform-specific; Android's settings page
  // is native UI and therefore isn't available in a tab for unit tests.
  //
  // TODO(mkwst): Determine how to reasonably test this on that platform.
  void NavigateToPasswordManagerSettingsPage() override;
  bool navigated_to_settings_page() const {
    return navigated_to_settings_page_;
  }

  // We don't have a FormManager in tests, so stub these out.
  void SavePassword() override;
  bool saved_password() const { return saved_password_; }

  void UpdatePassword(const autofill::PasswordForm& password_form) override;
  bool updated_password() const { return updated_password_; }

  void NeverSavePassword() override;
  bool never_saved_password() const { return never_saved_password_; }

  void ChooseCredential(const autofill::PasswordForm& form,
                        password_manager::CredentialType form_type) override;
  bool choose_credential() const { return choose_credential_; }
  autofill::PasswordForm chosen_credential() { return chosen_credential_; }

  const autofill::PasswordForm& PendingPassword() const override;
  void SetPendingPassword(autofill::PasswordForm pending_password);

  password_manager::ui::State state() const override;
  void SetState(password_manager::ui::State state);
  void UnsetState();

  void ManageAccounts() override;
  bool manage_accounts() const { return manage_accounts_; }

  void UpdateBubbleAndIconVisibility() override;

  void UpdateAndroidAccountChooserInfoBarVisibility() override;

  // Simulate the pending password state. |best_matches| can't be empty.
  void PretendSubmittedPassword(
    ScopedVector<autofill::PasswordForm> best_matches);

  static scoped_ptr<password_manager::PasswordFormManager> CreateFormManager(
      password_manager::PasswordManagerClient* client,
      const autofill::PasswordForm& observed_form,
      ScopedVector<autofill::PasswordForm> best_matches);

 private:
  bool navigated_to_settings_page_;
  bool saved_password_;
  bool updated_password_;
  bool never_saved_password_;
  bool choose_credential_;
  bool manage_accounts_;
  bool state_overridden_;
  password_manager::ui::State state_;
  base::TimeDelta elapsed_;

  autofill::PasswordForm chosen_credential_;
  autofill::PasswordForm pending_password_;

  password_manager::StubPasswordManagerClient client_;
  password_manager::StubPasswordManagerDriver driver_;
  password_manager::PasswordManager password_manager_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsUIControllerMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_MOCK_H_
