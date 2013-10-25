// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace policy {
class PolicyOAuth2TokenFetcher;
}

namespace chromeos {

// WebUIMessageHandler implementation which handles events occurring on the
// page, such as the user pressing the signin button.
class EnrollmentScreenHandler
    : public BaseScreenHandler,
      public EnrollmentScreenActor,
      public BrowsingDataRemover::Observer {
 public:
  EnrollmentScreenHandler();
  virtual ~EnrollmentScreenHandler();

  // Implements WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  // Implements EnrollmentScreenActor:
  virtual void SetParameters(Controller* controller,
                             bool is_auto_enrollment,
                             bool can_exit_enrollment,
                             const std::string& user) OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void FetchOAuthToken() OVERRIDE;
  virtual void ResetAuth(const base::Closure& callback) OVERRIDE;
  virtual void ShowSigninScreen() OVERRIDE;
  virtual void ShowEnrollmentSpinnerScreen() OVERRIDE;
  virtual void ShowLoginSpinnerScreen() OVERRIDE;
  virtual void ShowAuthError(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void ShowEnrollmentStatus(policy::EnrollmentStatus status) OVERRIDE;
  virtual void ShowUIError(UIError error_code) OVERRIDE;

  // Implements BaseScreenHandler:
  virtual void Initialize() OVERRIDE;
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;

  // Implements BrowsingDataRemover::Observer:
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

 private:
  // Handlers for WebUI messages.
  void HandleClose(const std::string& reason);
  void HandleCompleteLogin(const std::string& user);
  void HandleRetry();

  // Shows a given enrollment step.
  void ShowStep(const char* step);

  // Display the given i18n resource as error message.
  void ShowError(int message_id, bool retry);

  // Display the given string as error message.
  void ShowErrorMessage(const std::string& message, bool retry);

  // Display the given i18n string as a progress message.
  void ShowWorking(int message_id);

  // Handles completion of the OAuth2 token fetch attempt.
  void OnTokenFetched(const std::string& token,
                      const GoogleServiceAuthError& error);

  // Shows the screen.
  void DoShow();

  // Keeps the controller for this actor.
  Controller* controller_;

  bool show_on_init_;

  // Whether this is an auto-enrollment screen.
  bool is_auto_enrollment_;

  // True of we can exit enrollment and return back to the regular login flow.
  bool can_exit_enrollment_;

  // Whether an enrollment attempt has failed.
  bool enrollment_failed_once_;

  // Username of the user signing in.
  std::string user_;

  // This intentionally lives here and not in the controller, since it needs to
  // execute requests in the context of the profile that displays the webui.
  scoped_ptr<policy::PolicyOAuth2TokenFetcher> oauth_fetcher_;

  // The browsing data remover instance currently active, if any.
  BrowsingDataRemover* browsing_data_remover_;

  // The callbacks to invoke after browsing data has been cleared.
  std::vector<base::Closure> auth_reset_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENROLLMENT_SCREEN_HANDLER_H_
