// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/error_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class CaptivePortalWindowProxy;
class NativeWindowDelegate;
class NetworkStateInformer;

// A class that handles the WebUI hooks in error screen.
class ErrorScreenHandler : public BaseScreenHandler,
                           public ErrorScreenActor {
 public:
  ErrorScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer);
  virtual ~ErrorScreenHandler();

  // ErrorScreenActor implementation:
  virtual void Show(OobeDisplay::Screen parent_screen,
                    base::DictionaryValue* params) OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void FixCaptivePortal() OVERRIDE;
  virtual void ShowCaptivePortal() OVERRIDE;
  virtual void HideCaptivePortal() OVERRIDE;
  virtual void SetUIState(ErrorScreen::UIState ui_state) OVERRIDE;
  virtual void SetErrorState(ErrorScreen::ErrorState error_state,
                             const std::string& network) OVERRIDE;
  virtual void AllowGuestSignin(bool allowed) OVERRIDE;
  virtual void AllowOfflineLogin(bool allowed) OVERRIDE;

 private:
  // Sends notification that error message is shown.
  void NetworkErrorShown();

  bool GetScreenName(OobeUI::Screen screen, std::string* name) const;

  // WebUI message handlers.
  void HandleShowCaptivePortal(const base::ListValue* args);
  void HandleHideCaptivePortal(const base::ListValue* args);

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // Proxy which manages showing of the window for captive portal entering.
  scoped_ptr<CaptivePortalWindowProxy> captive_portal_window_proxy_;

  // Network state informer used to keep error screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // NativeWindowDelegate used to get reference to NativeWindow.
  NativeWindowDelegate* native_window_delegate_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  DISALLOW_COPY_AND_ASSIGN(ErrorScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
