// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "net/base/net_errors.h"

namespace chromeos {

class SigninScreenHandler;

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

  explicit GaiaScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer);
  virtual ~GaiaScreenHandler();

  void LoadGaia(const GaiaContext& context);
  void UpdateGaia(const GaiaContext& context);
  void ReloadGaia();

  FrameState frame_state() const { return frame_state_; }
  net::Error frame_error() const { return frame_error_; }

 private:
  // TODO (ygorshenin@): remove this dependency.
  friend class SigninScreenHandler;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // WebUI message handlers.
  void HandleFrameLoadingCompleted(int status);

  // TODO (ygorshenin@): GaiaScreenHandler should implement
  // NetworkStateInformer::Observer.
  void UpdateState(ErrorScreenActor::ErrorReason reason);

  // TODO (ygorshenin@): remove this dependency.
  void SetSigninScreenHandler(SigninScreenHandler* handler);

  // Current state of Gaia frame.
  FrameState frame_state_;

  // Latest Gaia frame error.
  net::Error frame_error_;

  // Network state informer used to keep signin screen up.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Non-owning ptr to SigninScreenHandler instance. Should not be used
  // in dtor.
  // TODO (ygorshenin@): GaiaScreenHandler shouldn't communicate with
  // signin_screen_handler directly.
  SigninScreenHandler* signin_screen_handler_;

  DISALLOW_COPY_AND_ASSIGN(GaiaScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_GAIA_SCREEN_HANDLER_H_
