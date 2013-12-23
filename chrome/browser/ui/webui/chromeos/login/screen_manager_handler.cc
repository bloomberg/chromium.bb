// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/screen_manager_handler.h"

namespace {

const char kJsApiButtonPressed[] = "buttonPressed";
const char kJsApiContextChanged[] = "contextChanged";

}  // namespace

namespace chromeos {

ScreenManagerHandler::ScreenManagerHandler()
    : delegate_(NULL) {
}

ScreenManagerHandler::~ScreenManagerHandler() {
}

void ScreenManagerHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void ScreenManagerHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
}

void ScreenManagerHandler::Initialize() {
}

void ScreenManagerHandler::RegisterMessages() {
  AddCallback(kJsApiButtonPressed,
              &ScreenManagerHandler::HandleButtonPressed);
  AddCallback(kJsApiContextChanged,
              &ScreenManagerHandler::HandleContextChanged);
}

void ScreenManagerHandler::HandleButtonPressed(const std::string& screen_name,
                                               const std::string& button_id) {
  if (delegate_)
    delegate_->OnButtonPressed(screen_name, button_id);
}

void ScreenManagerHandler::HandleContextChanged(
    const std::string& screen_name,
    const base::DictionaryValue* diff) {
  if (delegate_)
    delegate_->OnContextChanged(screen_name, diff);
}

}  // namespace chromeos
