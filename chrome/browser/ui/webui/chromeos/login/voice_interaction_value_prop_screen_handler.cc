// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/voice_interaction_value_prop_screen_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/voice_interaction_value_prop_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

const char kJsScreenPath[] = "login.VoiceInteractionValuePropScreen";

}  // namespace

namespace chromeos {

VoiceInteractionValuePropScreenHandler::VoiceInteractionValuePropScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_call_js_prefix(kJsScreenPath);
}

VoiceInteractionValuePropScreenHandler::
    ~VoiceInteractionValuePropScreenHandler() {
  if (screen_) {
    screen_->OnViewDestroyed(this);
  }
}

void VoiceInteractionValuePropScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("locale", g_browser_process->GetApplicationLocale());
  builder->Add("voiceInteractionValuePropNoThanksButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_NO_THANKS_BUTTON);
  builder->Add("voiceInteractionValuePropContinueButton",
               IDS_VOICE_INTERACTION_VALUE_PROP_CONTINUE_BUTTION);
}

void VoiceInteractionValuePropScreenHandler::Bind(
    VoiceInteractionValuePropScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
  if (page_is_ready())
    Initialize();
}

void VoiceInteractionValuePropScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void VoiceInteractionValuePropScreenHandler::Show() {
  if (!page_is_ready() || !screen_) {
    show_on_init_ = true;
    return;
  }

  ShowScreen(kScreenId);
}

void VoiceInteractionValuePropScreenHandler::Hide() {}

void VoiceInteractionValuePropScreenHandler::Initialize() {
  if (!screen_ || !show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

}  // namespace chromeos
