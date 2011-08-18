// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "base/values.h"

namespace chromeos {

BaseScreenHandler::BaseScreenHandler() : page_is_ready_(false) {
}

BaseScreenHandler::~BaseScreenHandler() {
}

void BaseScreenHandler::InitializeBase() {
  page_is_ready_ = true;
  Initialize();
}

void BaseScreenHandler::ShowScreen(const char* screen_name,
                                   const base::DictionaryValue* data) {
  if (!web_ui_)
    return;
  DictionaryValue screen_params;
  screen_params.SetString("id", screen_name);
  if (data)
    screen_params.SetWithoutPathExpansion("data", data->DeepCopy());
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.showScreen", screen_params);
}

}  // namespace chromeos
