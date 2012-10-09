// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

ErrorScreenHandler::ErrorScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : network_state_informer_(network_state_informer),
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

void ErrorScreenHandler::UpdateState(NetworkStateInformer::State state,
                                     const std::string& network_name,
                                     const std::string& reason,
                                     ConnectionType last_network_type) {
  LOG(ERROR) << "ErrorScreenHandler::UpdateState(): state=" << state <<
      ", network_name=" << network_name <<
      ", reason=" << reason <<
      ", last_network_type=" << last_network_type;
  // TODO (ygorshenin): instead of just call JS function, move all
  // logic from JS here.
  base::FundamentalValue state_value(state);
  base::StringValue network_value(network_name);
  base::StringValue reason_value(reason);
  base::FundamentalValue last_network_value(last_network_type);
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.updateState",
      state_value, network_value, reason_value, last_network_value);
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
}

void ErrorScreenHandler::HandleFixCaptivePortal(const base::ListValue* args) {
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

void ErrorScreenHandler::HandleShowCaptivePortal(const base::ListValue* args) {
  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  HandleFixCaptivePortal(args);
  captive_portal_window_proxy_->Show();
}

void ErrorScreenHandler::HandleHideCaptivePortal(const base::ListValue* args) {
  if (captive_portal_window_proxy_.get())
    captive_portal_window_proxy_->Close();
}

}  // namespace chromeos
