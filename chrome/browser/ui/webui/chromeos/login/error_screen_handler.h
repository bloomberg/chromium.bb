// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class CaptivePortalWindowProxy;
class NativeWindowDelegate;
class NetworkStateInformer;

// A class that handles the WebUI hooks in error screen.
class ErrorScreenHandler
    : public BaseScreenHandler,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  enum State {
    STATE_UNKNOWN = 0,
    STATE_PROXY_ERROR,
    STATE_CAPTIVE_PORTAL_ERROR,
    STATE_OFFLINE_ERROR,
  };

  // Possible error reasons.
  static const char kErrorReasonProxyAuthCancelled[];
  static const char kErrorReasonProxyConnectionFailed[];
  static const char kErrorReasonProxyConfigChanged[];
  static const char kErrorReasonLoadingTimeout[];
  static const char kErrorReasonPortalDetected[];
  // Reason for a case when network manager notifies about network
  // change.
  static const char kErrorReasonNetworkChanged[];
  // Reason for a case when JS side requires error screen update.
  static const char kErrorReasonUpdate[];

  ErrorScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer);
  virtual ~ErrorScreenHandler();

  void SetNativeWindowDelegate(NativeWindowDelegate* native_window_delegate);

  void Show();

  void Hide();

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  virtual void UpdateState(NetworkStateInformer::State state,
                           const std::string& network_name,
                           const std::string& reason,
                           ConnectionType last_network_type) OVERRIDE;

 private:
  // Shows or hides offline message based on network {on|off}line
  // state and updates internal state.
  void UpdateStateInternal(NetworkStateInformer::State state,
                           std::string network,
                           std::string reason,
                           ConnectionType last_network_type,
                           bool force_update);

  // Reloads gaia frame.
  void ReloadGaiaScreen();

  // Clears error bubble.
  void ClearOobeErrors();

  void OnBeforeShow(ConnectionType last_network_type);
  void OnBeforeHide();

  // Fixes captive portal dialog.
  void FixCaptivePortal();

  // Shows captive portal dialog.
  void ShowCaptivePortal();

  // Hides captive portal dialog.
  void HideCaptivePortal();

  // Sends notification that error message is shown.
  void NetworkErrorShown();

  // Each of the following methods shows corresponsing error message
  // and updates internal state.
  void ShowProxyError();
  void ShowCaptivePortalError(const std::string& network);
  void ShowOfflineError();
  void ShowOfflineMessage(bool visible);

  OobeUI::Screen GetCurrentScreen();

  State get_state() { return state_; }
  void set_state(State state) { state_ = state; }

  bool is_shown() { return is_shown_; }
  void set_is_shown(bool is_shown) { is_shown_ = is_shown; }

  bool is_first_update_state_call() { return is_first_update_state_call_; }
  void set_is_first_update_state_call(bool is_first_update_state_call) {
    is_first_update_state_call_ = is_first_update_state_call;
  }

  bool gaia_is_local() { return gaia_is_local_; }
  void set_gaia_is_local(bool gaia_is_local) { gaia_is_local_ = gaia_is_local; }

  NetworkStateInformer::State last_network_state() {
    return last_network_state_;
  }
  void set_last_network_state(NetworkStateInformer::State state) {
    last_network_state_ = state;
  }

  // WebUI message handlers.
  void HandleFixCaptivePortal(const base::ListValue* args);
  void HandleShowCaptivePortal(const base::ListValue* args);
  void HandleHideCaptivePortal(const base::ListValue* args);
  void HandleErrorScreenUpdate(const base::ListValue* args);
  void HandleShowLoadingTimeoutError(const base::ListValue* args);
  void HandleUpdateGaiaIsLocal(const base::ListValue* args);

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

  bool is_shown_;
  bool is_first_update_state_call_;
  State state_;
  bool gaia_is_local_;
  NetworkStateInformer::State last_network_state_;

  base::CancelableClosure update_state_closure_;
  base::CancelableClosure connecting_closure_;

  // Proxy which manages showing of the window for captive portal entering.
  scoped_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;

  // Network state informer used to keep error message screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // NativeWindowDelegate used to get reference to NativeWindow.
  NativeWindowDelegate* native_window_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ErrorScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
