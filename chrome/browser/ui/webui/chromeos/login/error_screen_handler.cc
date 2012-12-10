// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Timeout to delay first notification about offline state for a
// current network.
const int kOfflineTimeoutSec = 5;

// Timeout used to prevent infinite connecting to a flaky network.
const int kConnectingTimeoutSec = 60;

bool IsProxyError(const std::string& reason) {
  return reason == ErrorScreenHandler::kErrorReasonProxyAuthCancelled ||
      reason == ErrorScreenHandler::kErrorReasonProxyConnectionFailed;
}

}  // namespace

// static
const char ErrorScreenHandler::kErrorReasonProxyAuthCancelled[] =
    "frame error:111";
const char ErrorScreenHandler::kErrorReasonProxyConnectionFailed[] =
    "frame error:130";
const char ErrorScreenHandler::kErrorReasonProxyConfigChanged[] =
    "proxy changed";
const char ErrorScreenHandler::kErrorReasonLoadingTimeout[] = "loading timeout";
const char ErrorScreenHandler::kErrorReasonPortalDetected[] = "portal detected";
const char ErrorScreenHandler::kErrorReasonNetworkChanged[] = "network changed";
const char ErrorScreenHandler::kErrorReasonUpdate[] = "update";

ErrorScreenHandler::ErrorScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : is_shown_(false),
      is_first_update_state_call_(true),
      state_(STATE_UNKNOWN),
      gaia_is_local_(false),
      last_network_state_(NetworkStateInformer::UNKNOWN),
      network_state_informer_(network_state_informer),
      native_window_delegate_(NULL) {
  DCHECK(network_state_informer_);
  network_state_informer_->AddObserver(this);
}

ErrorScreenHandler::~ErrorScreenHandler() {
  network_state_informer_->RemoveObserver(this);
}

void ErrorScreenHandler::SetNativeWindowDelegate(
    NativeWindowDelegate* native_window_delegate) {
  native_window_delegate_ = native_window_delegate;
}

void ErrorScreenHandler::Show() {
  ShowOfflineMessage(true);
  set_is_shown(true);
  NetworkErrorShown();
}

void ErrorScreenHandler::Hide() {
  ShowOfflineMessage(false);
  set_is_shown(false);
}

void ErrorScreenHandler::UpdateState(NetworkStateInformer::State state,
                                     const std::string& network_name,
                                     const std::string& reason,
                                     ConnectionType last_network_type) {
  LOG(INFO) << "ErrorScreenHandler::UpdateState(): state=" << state << ", "
            << "network_name=" << network_name << ", reason=" << reason << ", "
            << "last_network_type=" << last_network_type;
  UpdateStateInternal(state, network_name, reason, last_network_type, false);
}

void ErrorScreenHandler::UpdateStateInternal(NetworkStateInformer::State state,
                                             std::string network_name,
                                             std::string reason,
                                             ConnectionType last_network_type,
                                             bool force_update) {
  update_state_closure_.Cancel();
  if (state == NetworkStateInformer::OFFLINE && is_first_update_state_call()) {
    set_is_first_update_state_call(false);
    update_state_closure_.Reset(
        base::Bind(
            &ErrorScreenHandler::UpdateStateInternal,
            base::Unretained(this),
            state, network_name, reason, last_network_type, force_update));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        update_state_closure_.callback(),
        base::TimeDelta::FromSeconds(kOfflineTimeoutSec));
    return;
  }
  set_is_first_update_state_call(false);

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_closure_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_closure_.Reset(
          base::Bind(&ErrorScreenHandler::UpdateStateInternal,
                     base::Unretained(this),
                     state, network_name, reason, last_network_type, true));
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          connecting_closure_.callback(),
          base::TimeDelta::FromSeconds(kConnectingTimeoutSec));
    }
    return;
  }
  connecting_closure_.Cancel();

  bool is_online = (state == NetworkStateInformer::ONLINE);
  bool is_under_captive_portal =
      (state == NetworkStateInformer::CAPTIVE_PORTAL);
  bool is_proxy_error = IsProxyError(reason);
  bool is_timeout = false;
  bool is_gaia_signin = (GetCurrentScreen() == OobeUI::SCREEN_GAIA_SIGNIN);
  bool is_gaia_reloaded = false;
  bool should_overlay = is_gaia_signin && !gaia_is_local();

  // Reload frame if network is changed.
  if (reason == kErrorReasonNetworkChanged) {
    if (is_online &&
        last_network_state() != NetworkStateInformer::ONLINE &&
        is_gaia_signin && !is_gaia_reloaded) {
      ReloadGaiaScreen();
      is_gaia_reloaded = true;
    }
  }
  set_last_network_state(state);

  if (reason == kErrorReasonProxyConfigChanged && should_overlay &&
      is_gaia_signin && !is_gaia_reloaded) {
    // Schedules a immediate retry.
    ReloadGaiaScreen();
    is_gaia_reloaded = true;
    LOG(WARNING) << "Retry page load since proxy settings has been changed";
  }

  // Fake portal state for loading timeout.
  if (reason == kErrorReasonLoadingTimeout) {
    is_online = false;
    is_under_captive_portal = true;
    is_timeout = true;
  }

  // Portal was detected via generate_204 redirect on Chrome side.
  // Subsequent call to show dialog if it's already shown does nothing.
  if (reason == kErrorReasonPortalDetected) {
    is_online = false;
    is_under_captive_portal = true;
  }

  if (!is_online && should_overlay) {
    LOG(WARNING) << "Show offline message: state=" << state << ", "
                 << "network_name=" << network_name << ", "
                 << "reason=" << reason << ", "
                 << "is_under_captive_portal=" << is_under_captive_portal;
    ClearOobeErrors();
    OnBeforeShow(last_network_type);

    if (is_under_captive_portal && !is_proxy_error) {
      // Do not bother a user with obsessive captive portal showing. This
      // check makes captive portal being shown only once: either when error
      // screen is shown for the first time or when switching from another
      // error screen (offline, proxy).
      if (!is_shown() || get_state() != STATE_CAPTIVE_PORTAL_ERROR) {
        // In case of timeout we're suspecting that network might be
        // a captive portal but would like to check that first.
        // Otherwise (signal from shill / generate_204 got redirected)
        // show dialog right away.
        if (is_timeout)
          FixCaptivePortal();
        else
          ShowCaptivePortal();
      }
    } else {
      HideCaptivePortal();
    }

    if (is_under_captive_portal) {
      if (is_proxy_error)
        ShowProxyError();
      else
        ShowCaptivePortalError(network_name);
    } else {
      ShowOfflineError();
    }
    Show();
  } else {
    HideCaptivePortal();

    if (is_shown()) {
      LOG(WARNING) << "Hide offline message. state=" << state << ", "
                   << "network_name=" << network_name << ", reason=" << reason;
      OnBeforeHide();
      Hide();

      // Forces a reload for Gaia screen on hiding error message.
      if (is_gaia_signin && !is_gaia_reloaded) {
        ReloadGaiaScreen();
        is_gaia_reloaded = true;
      }
    }
  }
}

void ErrorScreenHandler::ReloadGaiaScreen() {
  web_ui()->CallJavascriptFunction("login.GaiaSigninScreen.doReload");
}

void ErrorScreenHandler::ClearOobeErrors() {
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.clearErrors");
}

void ErrorScreenHandler::OnBeforeShow(ConnectionType last_network_type) {
  base::FundamentalValue last_network_type_value(last_network_type);
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.onBeforeShow",
                                   last_network_type_value);
}

void ErrorScreenHandler::OnBeforeHide() {
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.onBeforeHide");
}

void ErrorScreenHandler::FixCaptivePortal() {
  if (!native_window_delegate_)
    return;
  // TODO (ygorshenin): move error page and captive portal window
  // showing logic to C++ (currenly most of it is done on the JS
  // side).
  if (!captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(network_state_informer_.get(),
                                     GetNativeWindow()));
  }
  captive_portal_window_proxy_->ShowIfRedirected();
}

void ErrorScreenHandler::ShowCaptivePortal() {
  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  FixCaptivePortal();
  captive_portal_window_proxy_->Show();
}

void ErrorScreenHandler::HideCaptivePortal() {
  if (captive_portal_window_proxy_.get())
    captive_portal_window_proxy_->Close();
}

void ErrorScreenHandler::NetworkErrorShown() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void ErrorScreenHandler::ShowProxyError() {
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.showProxyError");
  set_state(STATE_PROXY_ERROR);
}

void ErrorScreenHandler::ShowCaptivePortalError(const std::string& network) {
  base::StringValue network_value(network);
  web_ui()->CallJavascriptFunction(
      "login.ErrorMessageScreen.showCaptivePortalError", network_value);
  set_state(STATE_CAPTIVE_PORTAL_ERROR);
}

void ErrorScreenHandler::ShowOfflineError() {
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.showOfflineError");
  set_state(STATE_OFFLINE_ERROR);
}

void ErrorScreenHandler::ShowOfflineMessage(bool visible) {
  base::FundamentalValue visible_value(visible);
  web_ui()->CallJavascriptFunction(
      "login.ErrorMessageScreen.showOfflineMessage", visible_value);
}

OobeUI::Screen ErrorScreenHandler::GetCurrentScreen() {
  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (oobe_ui)
    screen = oobe_ui->current_screen();
  return screen;
}

void ErrorScreenHandler::HandleFixCaptivePortal(const base::ListValue* args) {
  FixCaptivePortal();
}

void ErrorScreenHandler::HandleShowCaptivePortal(const base::ListValue* args) {
  ShowCaptivePortal();
}

void ErrorScreenHandler::HandleHideCaptivePortal(const base::ListValue* args) {
  HideCaptivePortal();
}

void ErrorScreenHandler::HandleErrorScreenUpdate(const base::ListValue* args) {
  UpdateStateInternal(network_state_informer_->state(),
                      network_state_informer_->network_name(),
                      kErrorReasonUpdate,
                      network_state_informer_->last_network_type(),
                      false);
}

void ErrorScreenHandler::HandleShowLoadingTimeoutError(
    const base::ListValue* args) {
  UpdateStateInternal(network_state_informer_->state(),
                      network_state_informer_->network_name(),
                      kErrorReasonLoadingTimeout,
                      network_state_informer_->last_network_type(),
                      false);
}

void ErrorScreenHandler::HandleUpdateGaiaIsLocal(const base::ListValue* args) {
  DCHECK(args && args->GetSize() == 1);

  bool gaia_is_local = false;
  if (!args->GetBoolean(0, &gaia_is_local))
    return;
  set_gaia_is_local(gaia_is_local);
}

void ErrorScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("fixCaptivePortal",
      base::Bind(&ErrorScreenHandler::HandleFixCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showCaptivePortal",
      base::Bind(&ErrorScreenHandler::HandleShowCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("hideCaptivePortal",
      base::Bind(&ErrorScreenHandler::HandleHideCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("errorScreenUpdate",
      base::Bind(&ErrorScreenHandler::HandleErrorScreenUpdate,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showLoadingTimeoutError",
      base::Bind(&ErrorScreenHandler::HandleShowLoadingTimeoutError,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("updateGaiaIsLocal",
      base::Bind(&ErrorScreenHandler::HandleUpdateGaiaIsLocal,
                 base::Unretained(this)));
}

void ErrorScreenHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("offlineMessageTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_TITLE));
  localized_strings->SetString("offlineMessageBody",
      l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_MESSAGE));
  localized_strings->SetString("captivePortalTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE));
  localized_strings->SetString("captivePortalMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL));
  localized_strings->SetString("captivePortalProxyMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY));
  localized_strings->SetString("captivePortalNetworkSelect",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT));
  localized_strings->SetString("proxyMessageText",
      l10n_util::GetStringUTF16(IDS_LOGIN_PROXY_ERROR_MESSAGE));
}

void ErrorScreenHandler::Initialize() {
}

gfx::NativeWindow ErrorScreenHandler::GetNativeWindow() {
  if (native_window_delegate_)
    return native_window_delegate_->GetNativeWindow();
  return NULL;
}

}  // namespace chromeos
