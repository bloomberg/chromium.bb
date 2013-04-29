// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/base_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"

namespace chromeos {

BaseScreen::BaseScreen() {
}

BaseScreen::~BaseScreen() {
}

void BaseScreen::Initialize(ScreenContext* context) {
}

void BaseScreen::OnShow() {
}

void BaseScreen::OnHide() {
}

void BaseScreen::OnClose() {
}

bool BaseScreen::IsStatusAreaDisplayed() {
  return true;
}

bool BaseScreen::IsPermanent() {
  return false;
}

std::string BaseScreen::GetID() const {
  return GetName();
}

void BaseScreen::Finish(const std::string& outcome) {

}

void BaseScreen::SetContext(ScreenContext* context) {
}

void BaseScreen::OnButtonPressed(const std::string& button_id) {
  LOG(WARNING) << "BaseScreen::OnButtonPressed(): button_id=" << button_id;
}

void BaseScreen::OnContextChanged(const base::DictionaryValue* diff) {
  LOG(WARNING) << "BaseScreen::OnContextChanged()";
}

}  // namespace chromeos
