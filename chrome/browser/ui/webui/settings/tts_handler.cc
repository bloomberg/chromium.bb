// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/tts_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/tts_controller_impl.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

void TtsHandler::HandleGetAllTtsVoiceData(const base::ListValue* args) {
  OnVoicesChanged();
}

void TtsHandler::OnVoicesChanged() {
  TtsControllerImpl* impl = TtsControllerImpl::GetInstance();
  std::vector<VoiceData> voices;
  impl->GetVoices(Profile::FromWebUI(web_ui()), &voices);
  base::ListValue responses;
  for (const auto& voice : voices) {
    base::DictionaryValue response;
    std::string language_code = l10n_util::GetLanguage(voice.lang);
    response.SetPath({"name"}, base::Value(voice.name));
    response.SetPath({"languageCode"}, base::Value(language_code));
    response.SetPath({"extensionId"}, base::Value(voice.extension_id));
    response.SetPath(
        {"displayLanguage"},
        base::Value(l10n_util::GetDisplayNameForLocale(
            language_code, g_browser_process->GetApplicationLocale(), true)));
    responses.GetList().push_back(std::move(response));
  }
  AllowJavascript();
  CallJavascriptFunction("cr.webUIListenerCallback",
                         base::Value("all-voice-data-updated"), responses);
}

void TtsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAllTtsVoiceData",
      base::BindRepeating(&TtsHandler::HandleGetAllTtsVoiceData,
                          base::Unretained(this)));
}

}  // namespace settings
