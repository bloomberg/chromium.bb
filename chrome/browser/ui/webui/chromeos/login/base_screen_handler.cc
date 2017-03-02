// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace {
const char kMethodContextChanged[] = "contextChanged";
}  // namespace

JSCallsContainer::JSCallsContainer() = default;

JSCallsContainer::~JSCallsContainer() = default;

BaseScreenHandler::BaseScreenHandler() = default;

BaseScreenHandler::BaseScreenHandler(JSCallsContainer* js_calls_container)
    : js_calls_container_(js_calls_container) {}

BaseScreenHandler::~BaseScreenHandler() {
  if (base_screen_)
    base_screen_->set_model_view_channel(nullptr);
}

void BaseScreenHandler::InitializeBase() {
  page_is_ready_ = true;
  Initialize();
  if (!pending_context_changes_.empty()) {
    CommitContextChanges(pending_context_changes_);
    pending_context_changes_.Clear();
  }
}

void BaseScreenHandler::GetLocalizedStrings(base::DictionaryValue* dict) {
  auto builder = base::MakeUnique<::login::LocalizedValuesBuilder>(dict);
  DeclareLocalizedValues(builder.get());
  GetAdditionalParameters(dict);
}

void BaseScreenHandler::RegisterMessages() {
  AddPrefixedCallback("userActed",
                      &BaseScreenHandler::HandleUserAction);
  AddPrefixedCallback("contextChanged",
                      &BaseScreenHandler::HandleContextChanged);
  DeclareJSCallbacks();
}

void BaseScreenHandler::CommitContextChanges(
    const base::DictionaryValue& diff) {
  if (!page_is_ready())
    pending_context_changes_.MergeDictionary(&diff);
  else
    CallJS(kMethodContextChanged, diff);
}

void BaseScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
}

void BaseScreenHandler::CallJS(const std::string& method) {
  web_ui()->CallJavascriptFunctionUnsafe(FullMethodPath(method));
}

void BaseScreenHandler::ShowScreen(OobeScreen screen) {
  ShowScreenWithData(screen, nullptr);
}

void BaseScreenHandler::ShowScreenWithData(OobeScreen screen,
                                           const base::DictionaryValue* data) {
  if (!web_ui())
    return;
  base::DictionaryValue screen_params;
  screen_params.SetString("id", GetOobeScreenName(screen));
  if (data)
    screen_params.SetWithoutPathExpansion("data", data->DeepCopy());
  web_ui()->CallJavascriptFunctionUnsafe("cr.ui.Oobe.showScreen",
                                         screen_params);
}

OobeUI* BaseScreenHandler::GetOobeUI() const {
  return static_cast<OobeUI*>(web_ui()->GetController());
}

OobeScreen BaseScreenHandler::GetCurrentScreen() const {
  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui)
    return OobeScreen::SCREEN_UNKNOWN;
  return oobe_ui->current_screen();
}

gfx::NativeWindow BaseScreenHandler::GetNativeWindow() {
  return LoginDisplayHost::default_host()->GetNativeWindow();
}

void BaseScreenHandler::SetBaseScreen(BaseScreen* base_screen) {
  if (base_screen_ == base_screen)
    return;
  if (base_screen_)
    base_screen_->set_model_view_channel(nullptr);
  base_screen_ = base_screen;
  if (base_screen_)
    base_screen_->set_model_view_channel(this);
}

std::string BaseScreenHandler::FullMethodPath(const std::string& method) const {
  DCHECK(!method.empty());
  return js_screen_path_prefix_ + method;
}

void BaseScreenHandler::HandleUserAction(const std::string& action_id) {
  if (base_screen_)
    base_screen_->OnUserAction(action_id);
}

void BaseScreenHandler::HandleContextChanged(
    const base::DictionaryValue* diff) {
  if (diff && base_screen_)
    base_screen_->OnContextChanged(*diff);
}

void BaseScreenHandler::ExecuteDeferredJSCalls() {
  DCHECK(!js_calls_container_->is_initialized());
  js_calls_container_->mark_initialized();
  for (const auto& deferred_js_call : js_calls_container_->deferred_js_calls())
    deferred_js_call.Run();
  js_calls_container_->deferred_js_calls().clear();
}

}  // namespace chromeos
