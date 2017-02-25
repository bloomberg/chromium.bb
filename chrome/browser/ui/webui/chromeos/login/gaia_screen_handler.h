// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_view.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "net/base/net_errors.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class AccountId;

namespace authpolicy {
class ActiveDirectoryAccountData;
}

namespace chromeos {

class Key;
class SigninScreenHandler;
class SigninScreenHandlerDelegate;

// A class that handles WebUI hooks in Gaia screen.
class GaiaScreenHandler : public BaseScreenHandler,
                          public GaiaView,
                          public NetworkPortalDetector::Observer {
 public:
  enum FrameState {
    FRAME_STATE_UNKNOWN = 0,
    FRAME_STATE_LOADING,
    FRAME_STATE_LOADED,
    FRAME_STATE_ERROR
  };

  GaiaScreenHandler(
      CoreOobeView* core_oobe_view,
      const scoped_refptr<NetworkStateInformer>& network_state_informer);
  ~GaiaScreenHandler() override;

  // GaiaView:
  void MaybePreloadAuthExtension() override;
  void DisableRestrictiveProxyCheckForTest() override;

 private:
  // TODO (antrim@): remove this dependency.
  friend class SigninScreenHandler;

  struct GaiaContext;

  void LoadGaia(const GaiaContext& context);

  // Callback that loads GAIA after version information has been retrieved.
  void LoadGaiaWithVersion(const GaiaContext& context,
                           const std::string& platform_version);

  // Sends request to reload Gaia. If |force_reload| is true, request
  // will be sent in any case, otherwise it will be sent only when Gaia is
  // not loading right now.
  void ReloadGaia(bool force_reload);

  // Turns offline idle detection on or off. Idle detection should only be on if
  // we're using the offline login page but the device is online.
  void MonitorOfflineIdle(bool is_online);

  // Show error UI at the end of GAIA flow when user is not whitelisted.
  void ShowWhitelistCheckFailedError();

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // NetworkPortalDetector::Observer implementation.
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  // WebUI message handlers.
  void HandleWebviewLoadAborted(const std::string& error_reason_str);
  void HandleCompleteAuthentication(const std::string& gaia_id,
                                    const std::string& email,
                                    const std::string& password,
                                    const std::string& auth_code,
                                    bool using_saml,
                                    const std::string& gaps_cookie);
  void HandleCompleteAuthenticationAuthCodeOnly(const std::string& auth_code);
  void HandleCompleteLogin(const std::string& gaia_id,
                           const std::string& typed_email,
                           const std::string& password,
                           bool using_saml);

  void HandleCompleteAdAuthentication(const std::string& username,
                                      const std::string& password);

  void HandleCompleteAdPasswordChange(const std::string& username,
                                      const std::string& old_password,
                                      const std::string& new_password);

  void HandleUsingSAMLAPI();
  void HandleScrapedPasswordCount(int password_count);
  void HandleScrapedPasswordVerificationFailed();

  void HandleGaiaUIReady();

  void HandleToggleEasyBootstrap();

  void HandleIdentifierEntered(const std::string& account_identifier);

  void HandleAuthExtensionLoaded();

  // Really handles the complete login message.
  void DoCompleteLogin(const std::string& gaia_id,
                       const std::string& typed_email,
                       const std::string& password,
                       bool using_saml);

  // Fill GAIA user name.
  void set_populated_email(const std::string& populated_email) {
    populated_email_ = populated_email;
  }

  // Kick off cookie / local storage cleanup.
  void StartClearingCookies(const base::Closure& on_clear_callback);
  void OnCookiesCleared(const base::Closure& on_clear_callback);

  // Kick off DNS cache flushing.
  void StartClearingDnsCache();
  void OnDnsCleared();

  // Callback for AuthPolicyClient.
  void DoAdAuth(const std::string& username,
                const Key& key,
                authpolicy::ErrorType error,
                const authpolicy::ActiveDirectoryAccountData& account_data);

  // Callback for writing password into pipe.
  void OnPasswordPipeReady(const std::string& username,
                           const Key& key,
                           base::ScopedFD password_fd);

  // Show sign-in screen for the given credentials.
  void ShowSigninScreenForTest(const std::string& username,
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
  void ShowGaiaAsync();

  // Cancels the request to show the sign-in screen while the asynchronous
  // clean-up process that precedes the screen showing is in progress.
  void CancelShowGaiaAsync();

  // Shows signin screen after dns cache and cookie cleanup operations finish.
  void ShowGaiaScreenIfReady();

  // Tells webui to load authentication extension. |force| is used to force the
  // extension reloading, if it has already been loaded. |offline| is true when
  // offline version of the extension should be used.
  void LoadAuthExtension(bool force, bool offline);

  // TODO (antrim@): GaiaScreenHandler should implement
  // NetworkStateInformer::Observer.
  void UpdateState(NetworkError::ErrorReason reason);

  // TODO (antrim@): remove this dependency.
  void set_signin_screen_handler(SigninScreenHandler* handler) {
    signin_screen_handler_ = handler;
  }

  // Are we on a restrictive proxy?
  bool IsRestrictiveProxy() const;

  SigninScreenHandlerDelegate* Delegate();

  // Returns temporary unused device Id.
  std::string GetTemporaryDeviceId();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

  // Returns user canonical e-mail. Finds already used account alias, if
  // user has already signed in.
  AccountId GetAccountId(const std::string& authenticated_email,
                         const std::string& id,
                         const AccountType& account_type) const;

  bool offline_login_is_active() const { return offline_login_is_active_; }
  void set_offline_login_is_active(bool offline_login_is_active) {
    offline_login_is_active_ = offline_login_is_active;
  }

  // Current state of Gaia frame.
  FrameState frame_state_ = FRAME_STATE_UNKNOWN;

  // Latest Gaia frame error.
  net::Error frame_error_ = net::OK;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  CoreOobeView* core_oobe_view_ = nullptr;

  // Email to pre-populate with.
  std::string populated_email_;

  // True if dns cache cleanup is done.
  bool dns_cleared_ = false;

  // True if DNS cache task is already running.
  bool dns_clear_task_running_ = false;

  // True if cookie jar cleanup is done.
  bool cookies_cleared_ = false;

  // If true, the sign-in screen will be shown when DNS cache and cookie
  // clean-up finish.
  bool show_when_dns_and_cookies_cleared_ = false;

  // Has Gaia page silent load been started for the current sign-in attempt?
  bool gaia_silent_load_ = false;

  // The active network at the moment when Gaia page was preloaded.
  std::string gaia_silent_load_network_;

  // If the user authenticated via SAML, this indicates whether the principals
  // API was used.
  bool using_saml_api_ = false;

  // Test credentials.
  std::string test_user_;
  std::string test_pass_;
  bool test_expects_complete_login_ = false;

  // True if Easy bootstrap is enabled.
  bool use_easy_bootstrap_ = false;

  // True if proxy doesn't allow access to google.com/generate_204.
  NetworkPortalDetector::CaptivePortalStatus captive_portal_status_ =
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;

  std::unique_ptr<NetworkPortalDetector> network_portal_detector_;
  bool disable_restrictive_proxy_check_for_test_ = false;

  // Non-owning ptr to SigninScreenHandler instance. Should not be used
  // in dtor.
  // TODO (antrim@): GaiaScreenHandler shouldn't communicate with
  // signin_screen_handler directly.
  SigninScreenHandler* signin_screen_handler_ = nullptr;

  // True if offline GAIA is active.
  bool offline_login_is_active_ = false;

  // True if the authentication extension is still loading.
  bool auth_extension_being_loaded_ = false;

  base::WeakPtrFactory<GaiaScreenHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GaiaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
