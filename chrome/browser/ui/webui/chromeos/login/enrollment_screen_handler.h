// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/screens/network_error_model.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "net/base/net_errors.h"

namespace chromeos {

class ErrorScreensHistogramHelper;
class HelpAppLauncher;

// WebUIMessageHandler implementation which handles events occurring on the
// page, such as the user pressing the signin button.
class EnrollmentScreenHandler
    : public BaseScreenHandler,
      public EnrollmentScreenActor,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  EnrollmentScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      NetworkErrorModel* network_error_model);
  ~EnrollmentScreenHandler() override;

  // Implements WebUIMessageHandler:
  void RegisterMessages() override;

  // Implements EnrollmentScreenActor:
  void SetParameters(Controller* controller,
                     const policy::EnrollmentConfig& config) override;
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void ShowSigninScreen() override;
  void ShowAttributePromptScreen(const std::string& asset_id,
                                 const std::string& location) override;
  void ShowEnrollmentSpinnerScreen() override;
  void ShowAuthError(const GoogleServiceAuthError& error) override;
  void ShowEnrollmentStatus(policy::EnrollmentStatus status) override;
  void ShowOtherError(
      EnterpriseEnrollmentHelper::OtherError error_code) override;

  // Implements BaseScreenHandler:
  void Initialize() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // Implements NetworkStateInformer::NetworkStateInformerObserver
  void UpdateState(NetworkError::ErrorReason reason) override;

 private:
  // Handlers for WebUI messages.
  void HandleToggleFakeEnrollment();
  void HandleClose(const std::string& reason);
  void HandleCompleteLogin(const std::string& user,
                           const std::string& auth_code);
  void HandleRetry();
  void HandleFrameLoadingCompleted();
  void HandleDeviceAttributesProvided(const std::string& asset_id,
                                      const std::string& location);
  void HandleOnLearnMore();

  void UpdateStateInternal(NetworkError::ErrorReason reason, bool force_update);
  void SetupAndShowOfflineMessage(NetworkStateInformer::State state,
                                  NetworkError::ErrorReason reason);
  void HideOfflineMessage(NetworkStateInformer::State state,
                          NetworkError::ErrorReason reason);

  // Shows a given enrollment step.
  void ShowStep(const char* step);

  // Display the given i18n resource as error message.
  void ShowError(int message_id, bool retry);

  // Display the given string as error message.
  void ShowErrorMessage(const std::string& message, bool retry);

  // Display the given i18n string as a progress message.
  void ShowWorking(int message_id);

  // Shows the screen.
  void DoShow();

  // Returns true if current visible screen is the enrollment sign-in page.
  bool IsOnEnrollmentScreen() const;

  // Returns true if current visible screen is the error screen over
  // enrollment sign-in page.
  bool IsEnrollmentScreenHiddenByError() const;

  // Keeps the controller for this actor.
  Controller* controller_;

  bool show_on_init_;

  // The enrollment configuration.
  policy::EnrollmentConfig config_;

  // True if screen was not shown yet.
  bool first_show_;

  // Whether we should handle network errors on enrollment screen.
  // True when signin screen step is shown.
  bool observe_network_failure_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  NetworkErrorModel* network_error_model_;

  std::unique_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  base::WeakPtrFactory<EnrollmentScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_
