// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
const char kMethodContextChanged[] = "contextChanged";
}  // namespace

BaseScreenHandler::BaseScreenHandler()
    : page_is_ready_(false), base_screen_(nullptr) {
}

BaseScreenHandler::BaseScreenHandler(const std::string& js_screen_path)
    : page_is_ready_(false),
      base_screen_(nullptr),
      js_screen_path_prefix_(js_screen_path + ".") {
  CHECK(!js_screen_path.empty());
}

BaseScreenHandler::~BaseScreenHandler() {
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
  auto builder = make_scoped_ptr(new ::login::LocalizedValuesBuilder(dict));
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
  web_ui()->CallJavascriptFunction(FullMethodPath(method));
}

void BaseScreenHandler::ShowScreen(const char* screen_name,
                                   const base::DictionaryValue* data) {
  if (!web_ui())
    return;
  base::DictionaryValue screen_params;
  screen_params.SetString("id", screen_name);
  if (data)
    screen_params.SetWithoutPathExpansion("data", data->DeepCopy());
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showScreen", screen_params);
}

gfx::NativeWindow BaseScreenHandler::GetNativeWindow() {
  return LoginDisplayHostImpl::default_host()->GetNativeWindow();
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

}  // namespace chromeos
