// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/screens/network_error_model.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/login/auth/login_performer.h"

namespace chromeos {

class BaseScreenDelegate;
class CaptivePortalWindowProxy;
class NetworkErrorView;

// Controller for the error screen.
class ErrorScreen : public NetworkErrorModel, public LoginPerformer::Delegate {
 public:
  typedef scoped_ptr<base::CallbackList<void()>::Subscription>
      ConnectRequestCallbackSubscription;

  ErrorScreen(BaseScreenDelegate* base_screen_delegate, NetworkErrorView* view);
  ~ErrorScreen() override;

  // NetworkErrorModel:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void OnShow() override;
  void OnHide() override;
  void OnUserAction(const std::string& action_id) override;
  void OnContextKeyUpdated(const ::login::ScreenContext::KeyType& key) override;
  void AllowGuestSignin(bool allowed) override;
  void AllowOfflineLogin(bool allowed) override;
  void FixCaptivePortal() override;
  NetworkError::UIState GetUIState() const override;
  NetworkError::ErrorState GetErrorState() const override;
  OobeUI::Screen GetParentScreen() const override;
  void HideCaptivePortal() override;
  void OnViewDestroyed(NetworkErrorView* view) override;
  void SetUIState(NetworkError::UIState ui_state) override;
  void SetErrorState(NetworkError::ErrorState error_state,
                     const std::string& network) override;
  void SetParentScreen(OobeUI::Screen parent_screen) override;
  void SetHideCallback(const base::Closure& on_hide) override;
  void ShowCaptivePortal() override;
  void ShowConnectingIndicator(bool show) override;

  // LoginPerformer::Delegate implementation:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnOffTheRecordAuthSuccess() override;
  void OnPasswordChangeDetected() override;
  void WhiteListCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void SetAuthFlowOffline(bool offline) override;

  // Register a callback to be invoked when the user indicates that an attempt
  // to connect to the network should be made.
  ConnectRequestCallbackSubscription RegisterConnectRequestCallback(
      const base::Closure& callback);

 private:
  // Default hide_closure for Hide().
  void DefaultHideCallback();

  // Handle user action to configure certificates.
  void OnConfigureCerts();

  // Handle user action to diagnose network configuration.
  void OnDiagnoseButtonClicked();

  // Handle user action to launch guest session from out-of-box.
  void OnLaunchOobeGuestSession();

  // Handle user action to launch Powerwash in case of
  // Local State critical error.
  void OnLocalStateErrorPowerwashButtonClicked();

  // Handle uses action to reboot device.
  void OnRebootButtonClicked();

  // The user indicated to make an attempt to connect to the network.
  void OnConnectRequested();

  // Handles the response of an ownership check and starts the guest session if
  // applicable.
  void StartGuestSessionAfterOwnershipCheck(
      DeviceSettingsService::OwnershipStatus ownership_status);

  NetworkErrorView* view_;

  scoped_ptr<LoginPerformer> guest_login_performer_;

  // Proxy which manages showing of the window for captive portal entering.
  scoped_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;

  // Network state informer used to keep error screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  NetworkError::UIState ui_state_;
  NetworkError::ErrorState error_state_;

  OobeUI::Screen parent_screen_;

  // Optional callback that is called when NetworkError screen is hidden.
  scoped_ptr<base::Closure> on_hide_callback_;

  // Callbacks to be invoked when a connection attempt is requested.
  base::CallbackList<void()> connect_request_callbacks_;

  base::WeakPtrFactory<ErrorScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ErrorScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_
