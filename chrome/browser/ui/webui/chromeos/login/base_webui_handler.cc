// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

#include <memory>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

JSCallsContainer::JSCallsContainer() = default;

JSCallsContainer::~JSCallsContainer() = default;

void JSCallsContainer::ExecuteDeferredJSCalls() {
  DCHECK(!is_initialized());
  is_initialized_ = true;
  // Copy deferred_js_calls_ into a separate variable to avoid any potential
  // concurrent modifications.
  auto calls = std::move(deferred_js_calls_);
  for (const auto& call : calls)
    call.Run();
  // We're initialized so no more calls should have been queued.
  // TODO(jdufault): Rework this class API so that this is not possible.
  DCHECK(deferred_js_calls_.empty());
}

BaseWebUIHandler::BaseWebUIHandler(JSCallsContainer* js_calls_container)
    : js_calls_container_(js_calls_container) {}

BaseWebUIHandler::~BaseWebUIHandler() {
  if (base_screen_)
    base_screen_->set_model_view_channel(nullptr);
}

void BaseWebUIHandler::InitializeBase() {
  page_is_ready_ = true;
  Initialize();
  if (!pending_context_changes_.empty()) {
    CommitContextChanges(pending_context_changes_);
    pending_context_changes_.Clear();
  }
}

void BaseWebUIHandler::GetLocalizedStrings(base::DictionaryValue* dict) {
  auto builder = std::make_unique<::login::LocalizedValuesBuilder>(dict);
  DeclareLocalizedValues(builder.get());
  GetAdditionalParameters(dict);
}

std::string BaseWebUIHandler::FullMethodPath(const std::string& method) const {
  DCHECK(!method.empty());
  return js_screen_path_prefix_ + method;
}

void BaseWebUIHandler::RegisterMessages() {
  AddCallback(FullMethodPath("userActed"),
              &BaseScreenHandler::HandleUserAction);
  AddCallback(FullMethodPath("contextChanged"),
              &BaseScreenHandler::HandleContextChanged);
  DeclareJSCallbacks();
}

void BaseWebUIHandler::CommitContextChanges(const base::DictionaryValue& diff) {
  if (!page_is_ready())
    pending_context_changes_.MergeDictionary(&diff);
  else
    CallJS(FullMethodPath("contextChanged"), diff);
}

void BaseWebUIHandler::GetAdditionalParameters(base::DictionaryValue* dict) {}

void BaseWebUIHandler::ShowScreen(OobeScreen screen) {
  ShowScreenWithData(screen, nullptr);
}

void BaseWebUIHandler::ShowScreenWithData(OobeScreen screen,
                                          const base::DictionaryValue* data) {
  if (!web_ui())
    return;
  base::DictionaryValue screen_params;
  screen_params.SetString("id", GetOobeScreenName(screen));
  if (data) {
    screen_params.SetKey("data", data->Clone());
  }
  CallJS("cr.ui.Oobe.showScreen", screen_params);
}

OobeUI* BaseWebUIHandler::GetOobeUI() const {
  return static_cast<OobeUI*>(web_ui()->GetController());
}

OobeScreen BaseWebUIHandler::GetCurrentScreen() const {
  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui)
    return OobeScreen::SCREEN_UNKNOWN;
  return oobe_ui->current_screen();
}

gfx::NativeWindow BaseWebUIHandler::GetNativeWindow() {
  return LoginDisplayHost::default_host()->GetNativeWindow();
}

void BaseWebUIHandler::SetBaseScreen(BaseScreen* base_screen) {
  if (base_screen_ == base_screen)
    return;
  if (base_screen_)
    base_screen_->set_model_view_channel(nullptr);
  base_screen_ = base_screen;
  if (base_screen_)
    base_screen_->set_model_view_channel(this);
}

void BaseWebUIHandler::HandleUserAction(const std::string& action_id) {
  if (base_screen_)
    base_screen_->OnUserAction(action_id);
}

void BaseWebUIHandler::HandleContextChanged(const base::DictionaryValue* diff) {
  if (diff && base_screen_)
    base_screen_->OnContextChanged(*diff);
}

}  // namespace chromeos
