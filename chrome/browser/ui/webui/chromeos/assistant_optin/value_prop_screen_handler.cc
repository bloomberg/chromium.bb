// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/value_prop_screen_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

const char kJsScreenPath[] = "AssistantValuePropScreen";

constexpr const char kUserActionSkipPressed[] = "skip-pressed";
constexpr const char kUserActionNextPressed[] = "next-pressed";

}  // namespace

namespace chromeos {

ValuePropScreenHandler::ValuePropScreenHandler(
    OnAssistantOptInScreenExitCallback callback)
    : BaseWebUIHandler(), exit_callback_(std::move(callback)) {
  set_call_js_prefix(kJsScreenPath);
}

ValuePropScreenHandler::~ValuePropScreenHandler() = default;

void ValuePropScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("locale", g_browser_process->GetApplicationLocale());
  builder->Add("voiceInteractionValuePropLoading",
               IDS_VOICE_INTERACTION_VALUE_PROP_LOADING);
  builder->Add("voiceInteractionValuePropLoadErrorTitle",
               IDS_VOICE_INTERACTION_VALUE_PROP_LOAD_ERROR_TITLE);
  builder->Add("voiceInteractionValuePropLoadErrorMessage",
               IDS_VOICE_INTERACTION_VALUE_PROP_LOAD_ERROR_MESSAGE);
  builder->Add("voiceInteractionValuePropSkipButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_SKIP_BUTTON);
  builder->Add("voiceInteractionValuePropRetryButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_RETRY_BUTTON);
  builder->Add("voiceInteractionValuePropNextButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_NEXT_BUTTION);

  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
}

void ValuePropScreenHandler::RegisterMessages() {
  AddPrefixedCallback("userActed", &ValuePropScreenHandler::HandleUserAction);
}

void ValuePropScreenHandler::Initialize() {}

void ValuePropScreenHandler::HandleUserAction(const std::string& action) {
  DCHECK(exit_callback_);
  if (action == kUserActionSkipPressed)
    std::move(exit_callback_)
        .Run(AssistantOptInScreenExitCode::VALUE_PROP_SKIPPED);
  else if (action == kUserActionNextPressed)
    std::move(exit_callback_)
        .Run(AssistantOptInScreenExitCode::VALUE_PROP_ACCEPTED);
}

}  // namespace chromeos
