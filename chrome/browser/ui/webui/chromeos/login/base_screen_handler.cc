// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

LocalizedValuesBuilder::LocalizedValuesBuilder(base::DictionaryValue* dict)
    : dict_(dict) {
}

void LocalizedValuesBuilder::Add(const std::string& key, int message_id) {
  dict_->SetString(key,
                   l10n_util::GetStringUTF16(message_id));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  const string16& a) {
  dict_->SetString(key,
                   l10n_util::GetStringFUTF16(message_id, a));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  const string16& a,
                                  const string16& b) {
  dict_->SetString(key,
                   l10n_util::GetStringFUTF16(message_id, a, b));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  int message_id_a) {
    AddF(key, message_id, l10n_util::GetStringUTF16(message_id_a));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  int message_id_a,
                                  int message_id_b) {
    AddF(key, message_id,
         l10n_util::GetStringUTF16(message_id_a),
         l10n_util::GetStringUTF16(message_id_b));
}

BaseScreenHandler::BaseScreenHandler() : page_is_ready_(false) {
}

BaseScreenHandler::~BaseScreenHandler() {
}

void BaseScreenHandler::InitializeBase() {
  page_is_ready_ = true;
  Initialize();
}

void BaseScreenHandler::GetLocalizedStrings(base::DictionaryValue* dict) {
  scoped_ptr<LocalizedValuesBuilder> builder(new LocalizedValuesBuilder(dict));
  DeclareLocalizedValues(builder.get());
  GetAdditionalParameters(dict);
}

void BaseScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
}

void BaseScreenHandler::CallJS(const std::string& method) {
  web_ui()->CallJavascriptFunction(method);
}

void BaseScreenHandler::CallJS(const std::string& method,
                               const base::Value& arg1) {
  web_ui()->CallJavascriptFunction(method, arg1);
}

void BaseScreenHandler::CallJS(const std::string& method,
                               const base::Value& arg1,
                               const base::Value& arg2) {
  web_ui()->CallJavascriptFunction(method, arg1, arg2);
}

void BaseScreenHandler::CallJS(const std::string& method,
                               const base::Value& arg1,
                               const base::Value& arg2,
                               const base::Value& arg3) {
  web_ui()->CallJavascriptFunction(method, arg1, arg2, arg3);
}

void BaseScreenHandler::CallJS(const std::string& method,
                               const base::Value& arg1,
                               const base::Value& arg2,
                               const base::Value& arg3,
                               const base::Value& arg4) {
  web_ui()->CallJavascriptFunction(method, arg1, arg2, arg3, arg4);
}

void BaseScreenHandler::ShowScreen(const char* screen_name,
                                   const base::DictionaryValue* data) {
  if (!web_ui())
    return;
  DictionaryValue screen_params;
  screen_params.SetString("id", screen_name);
  if (data)
    screen_params.SetWithoutPathExpansion("data", data->DeepCopy());
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.showScreen", screen_params);
}

gfx::NativeWindow BaseScreenHandler::GetNativeWindow() {
  return BaseLoginDisplayHost::default_host()->GetNativeWindow();
}

}  // namespace chromeos
