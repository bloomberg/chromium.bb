// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/extensions/signin/scoped_gaia_auth_extension.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "net/base/net_errors.h"

namespace policy {
class ConsumerManagementService;
}

namespace chromeos {

class SigninScreenHandler;
class SigninScreenHandlerDelegate;

// A class that's used to specify the way how Gaia should be loaded.
struct GaiaContext {
  GaiaContext();

  // Forces Gaia to reload.
  bool force_reload;

  // Whether local verison of Gaia is used.
  bool is_local;

  // True if password was changed for the current user.
  bool password_changed;

  // True if user pods can be displyed.
  bool show_users;

  // Whether Gaia should be loaded in offline mode.
  bool use_offline;

  // True if user list is non-empty.
  bool has_users;

  // Email of current user.
  std::string email;

  // Whether consumer management enrollment is in progress.
  bool is_enrolling_consumer_management;
};

// A class that handles WebUI hooks in Gaia screen.
class GaiaScreenHandler : public BaseScreenHandler {
 public:
  enum FrameState {
    FRAME_STATE_UNKNOWN = 0,
    FRAME_STATE_LOADING,
    FRAME_STATE_LOADED,
    FRAME_STATE_ERROR
  };

  GaiaScreenHandler(
      CoreOobeActor* core_oobe_actor,
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      policy::ConsumerManagementService* consumer_management);
  ~GaiaScreenHandler() override;

  void LoadGaia(const GaiaContext& context);
  void UpdateGaia(const GaiaContext& context);

  // Sends request to reload Gaia. If |force_reload| is true, request
  // will be sent in any case, otherwise it will be sent only when Gaia is
  // not loading right now.
  void ReloadGaia(bool force_reload);

  // Decides whether an auth extension should be pre-loaded. If it should,
  // pre-loads it.
  void MaybePreloadAuthExtension();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

 private:
  // TODO (antrim@): remove this dependency.
  friend class SigninScreenHandler;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // WebUI message handlers.
  void HandleFrameLoadingCompleted(int status);
  void HandleCompleteAuthentication(const std::string& gaia_id,
                                    const std::string& email,
                                    const std::string& password,
                                    const std::string& auth_code);
  void HandleCompleteAuthenticationAuthCodeOnly(const std::string& auth_code);
  void HandleCompleteLogin(const std::string& gaia_id,
                           const std::string& typed_email,
                           const std::string& password,
                           bool using_saml);

  void HandleUsingSAMLAPI();
  void HandleScrapedPasswordCount(int password_count);
  void HandleScrapedPasswordVerificationFailed();

  void HandleGaiaUIReady();

  void HandleToggleEasyBootstrap();

  void HandleToggleWebviewSignin();

  // This is called when ConsumerManagementService::SetOwner() returns.
  void OnSetOwnerDone(const std::string& gaia_id,
                      const std::string& typed_email,
                      const std::string& password,
                      bool using_saml,
                      bool success);

  // Really handles the complete login message.
  void DoCompleteLogin(const std::string& gaia_id,
                       const std::string& typed_email,
                       const std::string& password,
                       bool using_saml);

  // Fill GAIA user name.
  void PopulateEmail(const std::string& user_id);

  // Mark user as having password changed:
  void PasswordChangedFor(const std::string& user_id);

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies(const base::Closure& on_clear_callback);
  void OnCookiesCleared(const base::Closure& on_clear_callback);

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Show sign-in screen for the given credentials.
  virtual void ShowSigninScreenForCreds(const std::string& username,
                                        const std::string& password);
  // Attempts login for test.
  void SubmitLoginFormForTest();

  // Updates the member variable and UMA histogram indicating whether the
  // principals API was used during SAML login.
  void SetSAMLPrincipalsAPIUsed(bool api_used);

  // Show the sign-in screen. Depending on internal state, the screen will
  // either be shown immediately or after an asynchronous clean-up process that
  // cleans DNS cache and cookies. In the latter case, the request to show the
  // screen can be canceled by calling CancelShowGaiaAsync() while the clean-up
  // is in progress.
  void ShowGaiaAsync(bool is_enrolling_consumer_management);

  // Cancels the request to show the sign-in screen while the asynchronous
  // clean-up process that precedes the screen showing is in progress.
  void CancelShowGaiaAsync();

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowGaiaScreenIfReady();

  // Tells webui to load authentication extension. |force| is used to force the
  // extension reloading, if it has already been loaded. |silent_load| is true
  // for cases when extension should be loaded in the background and it
  // shouldn't grab the focus. |offline| is true when offline version of the
  // extension should be used.
  void LoadAuthExtension(bool force, bool silent_load, bool offline);

  // TODO (antrim@): GaiaScreenHandler should implement
  // NetworkStateInformer::Observer.
  void UpdateState(NetworkError::ErrorReason reason);

  // TODO (antrim@): remove this dependency.
  void SetSigninScreenHandler(SigninScreenHandler* handler);

  SigninScreenHandlerDelegate* Delegate();

  // Current state of Gaia frame.
  FrameState frame_state_;

  // Latest Gaia frame error.
  net::Error frame_error_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Consumer management service for checking if enrollment is in progress.
  policy::ConsumerManagementService* consumer_management_;

  CoreOobeActor* core_oobe_actor_;

  // Email to pre-populate with.
  std::string populated_email_;

  // Emails of the users, whose passwords have recently been changed.
  std::set<std::string> password_changed_for_;

  // True if dns cache cleanup is done.
  bool dns_cleared_;

  // True if DNS cache task is already running.
  bool dns_clear_task_running_;

  // True if cookie jar cleanup is done.
  bool cookies_cleared_;

  // If true, the sign-in screen will be shown when DNS cache and cookie
  // clean-up finish.
  bool show_when_dns_and_cookies_cleared_;

  // Is focus still stolen from Gaia page?
  bool focus_stolen_;

  // Has Gaia page silent load been started for the current sign-in attempt?
  bool gaia_silent_load_;

  // The active network at the moment when Gaia page was preloaded.
  std::string gaia_silent_load_network_;

  // If the user authenticated via SAML, this indicates whether the principals
  // API was used.
  bool using_saml_api_;

  // Whether consumer management enrollment is in progress.
  bool is_enrolling_consumer_management_;

  // Test credentials.
  std::string test_user_;
  std::string test_pass_;
  bool test_expects_complete_login_;

  // True if Easy bootstrap is enabled.
  bool use_easy_bootstrap_;

  // Non-owning ptr to SigninScreenHandler instance. Should not be used
  // in dtor.
  // TODO (antrim@): GaiaScreenHandler shouldn't communicate with
  // signin_screen_handler directly.
  SigninScreenHandler* signin_screen_handler_;

  // GAIA extension loader.
  scoped_ptr<ScopedGaiaAuthExtension> auth_extension_;

  base::WeakPtrFactory<GaiaScreenHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GaiaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
