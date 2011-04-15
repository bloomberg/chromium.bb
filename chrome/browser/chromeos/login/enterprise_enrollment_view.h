// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/web_page_view.h"
#include "chrome/browser/ui/webui/chromeos/enterprise_enrollment_ui.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "views/view.h"

class DictionaryValue;

namespace views {
class GridLayout;
class Label;
}

namespace chromeos {

class EnterpriseEnrollmentController;
class ScreenObserver;

// Implements the UI for the enterprise enrollment screen in OOBE.
class EnterpriseEnrollmentView : public views::View,
                                 public EnterpriseEnrollmentUI::Controller {
 public:
  explicit EnterpriseEnrollmentView(EnterpriseEnrollmentController* controller);
  virtual ~EnterpriseEnrollmentView();

  void set_editable_user(bool editable);

  // Initialize view controls and layout.
  void Init();

  // Switches to the confirmation screen.
  void ShowConfirmationScreen();

  // Show an authentication error.
  void ShowAuthError(const GoogleServiceAuthError& error);
  void ShowAccountError();
  void ShowFatalAuthError();
  void ShowFatalEnrollmentError();
  void ShowNetworkEnrollmentError();

  // EnterpriseEnrollmentUI::Controller implementation.
  virtual void OnAuthSubmitted(const std::string& user,
                               const std::string& password,
                               const std::string& captcha,
                               const std::string& access_code) OVERRIDE;
  virtual void OnAuthCancelled() OVERRIDE;
  virtual void OnConfirmationClosed() OVERRIDE;
  virtual bool GetInitialUser(std::string* user) OVERRIDE;

 private:
  // Updates the gaia login box.
  void UpdateGaiaLogin(const DictionaryValue& args);

  // Display the given i18n string as error message.
  void ShowError(int message_id);

  // Overriden from views::View:
  virtual void Layout() OVERRIDE;

  EnterpriseEnrollmentController* controller_;

  // Controls.
  WebPageDomView* enrollment_page_view_;

  bool editable_user_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENTERPRISE_ENROLLMENT_VIEW_H_
