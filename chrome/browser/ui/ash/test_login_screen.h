// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
#define CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_

#include <string>
#include <vector>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/interfaces/login_screen.mojom.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/test_login_screen_model.h"
#include "mojo/public/cpp/bindings/binding.h"

// Test implementation of ash's mojo LoginScreen interface.
//
// Registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class TestLoginScreen : public ash::mojom::LoginScreen,
                        public ash::LoginScreen {
 public:
  TestLoginScreen();
  ~TestLoginScreen() override;

  // ash:mojom::LoginScreen:
  void SetClient(ash::mojom::LoginScreenClientPtr client) override;
  void ShowLockScreen(ShowLockScreenCallback callback) override;
  void ShowLoginScreen(ShowLoginScreenCallback callback) override;
  void ShowErrorMessage(int32_t login_attempts,
                        const std::string& error_text,
                        const std::string& help_link_text,
                        int32_t help_topic_id) override;
  void ClearErrors() override;
  void IsReadyForPassword(IsReadyForPasswordCallback callback) override;
  void ShowKioskAppError(const std::string& message) override;
  void SetAddUserButtonEnabled(bool enable) override;
  void SetShutdownButtonEnabled(bool enable) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  void SetShowGuestButtonInOobe(bool show) override;
  void SetShowParentAccessButton(bool show) override;
  void SetShowParentAccessDialog(bool show) override;
  void FocusLoginShelf(bool reverse) override;
  void ShowParentAccessWidget(
      const AccountId& child_account_id,
      base::RepeatingCallback<void(bool success)> callback) override;

  // ash::LoginScreen:
  ash::LoginScreenModel* GetModel() override;

 private:
  void Bind(mojo::ScopedMessagePipeHandle handle);
  mojo::Binding<ash::mojom::LoginScreen> binding_{this};

  TestLoginScreenModel test_screen_model_;

  DISALLOW_COPY_AND_ASSIGN(TestLoginScreen);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
