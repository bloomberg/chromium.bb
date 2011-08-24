// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/enterprise_enrollment_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

// WebUIMessageHandler implementation which handles events occurring on the
// page, such as the user pressing the signin button.
class EnterpriseEnrollmentScreenHandler
    : public BaseScreenHandler,
      public EnterpriseEnrollmentScreenActor {
 public:
  EnterpriseEnrollmentScreenHandler();
  virtual ~EnterpriseEnrollmentScreenHandler();

  void SetupGaiaStrings();

  // Implements WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  // Implements EnterpriseEnrollmentScreenActor:
  virtual void SetController(Controller* controller);
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetEditableUser(bool editable) OVERRIDE;
  virtual void ShowConfirmationScreen() OVERRIDE;
  virtual void ShowAuthError(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void ShowAccountError() OVERRIDE;
  virtual void ShowFatalAuthError() OVERRIDE;
  virtual void ShowFatalEnrollmentError() OVERRIDE;
  virtual void ShowNetworkEnrollmentError() OVERRIDE;

  // Implements BaseScreenHandler:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;

 protected:
  // Implements BaseScreenHandler:
  virtual void Initialize() OVERRIDE;

  // Keeps the controller for this actor.
  Controller* controller_;

 private:
  // Handlers for WebUI messages.
  void HandleSubmitAuth(const base::ListValue* args);
  void HandleCancelAuth(const base::ListValue* args);
  void HandleConfirmationClose(const base::ListValue* args);

  // Display the given i18n string as error message.
  void ShowError(int message_id);
  // Updates the gaia login box.
  void UpdateGaiaLogin(const base::DictionaryValue& args);

  bool editable_user_;
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_SCREEN_HANDLER_H_
