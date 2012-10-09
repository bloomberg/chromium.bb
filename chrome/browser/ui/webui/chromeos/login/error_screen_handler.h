// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ERROR_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
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
class ErrorScreenHandler
    : public BaseScreenHandler,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  ErrorScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer);
  virtual ~ErrorScreenHandler();

  void SetNativeWindowDelegate(NativeWindowDelegate* native_window_delegate);

  // NetworkStateInformer::NetworkStateInformerObserver implementation:
  virtual void UpdateState(NetworkStateInformer::State state,
                           const std::string& network_name,
                           const std::string& reason,
                           ConnectionType last_network_type) OVERRIDE;

 private:
  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // WebUI message handlers.
  void HandleFixCaptivePortal(const base::ListValue* args);
  void HandleShowCaptivePortal(const base::ListValue* args);
  void HandleHideCaptivePortal(const base::ListValue* args);

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
