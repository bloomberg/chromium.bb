// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/base_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/model_view_channel.h"

namespace chromeos {

BaseScreen::BaseScreen(BaseScreenDelegate* base_screen_delegate)
    : channel_(nullptr), base_screen_delegate_(base_screen_delegate) {
}

BaseScreen::~BaseScreen() {
}

void BaseScreen::Initialize(::login::ScreenContext* context) {
  if (context)
    context_.CopyFrom(*context);
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
  // TODO (ygorshenin, crbug.com/433797): elimitate intermediate
  // GetName() ASAP.
  return GetName();
}

void BaseScreen::CommitContextChanges() {
  if (!context_.HasChanges())
    return;
  if (!channel_) {
    LOG(ERROR) << "Model-view channel for " << GetID()
               << " is not ready, context changes are not sent to the view.";
    return;
  }
  base::DictionaryValue diff;
  context_.GetChangesAndReset(&diff);
  channel_->CommitContextChanges(diff);
}

void BaseScreen::Finish(BaseScreenDelegate::ExitCodes exit_code) {
  base_screen_delegate_->OnExit(*this, exit_code, &context_);
}

void BaseScreen::SetContext(::login::ScreenContext* context) {
  if (context)
    context_.CopyFrom(*context);
}

void BaseScreen::OnUserAction(const std::string& action_id) {
  LOG(WARNING) << "Unhandled user action: action_id=" << action_id;
}

void BaseScreen::OnContextKeyUpdated(
    const ::login::ScreenContext::KeyType& key) {
  LOG(WARNING) << "Unhandled context change: key=" << key;
}

void BaseScreen::OnContextChanged(const base::DictionaryValue& diff) {
  std::vector<::login::ScreenContext::KeyType> keys;
  context_.ApplyChanges(diff, &keys);
  for (const auto& key : keys)
    OnContextKeyUpdated(key);
}

}  // namespace chromeos
