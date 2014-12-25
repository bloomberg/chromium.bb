// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/base_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/model_view_channel.h"

namespace chromeos {

BaseScreen::ContextEditor::ContextEditor(BaseScreen& screen)
    : screen_(screen), context_(screen.context_) {
}

BaseScreen::ContextEditor::~ContextEditor() {
  screen_.CommitContextChanges();
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetBoolean(
    const KeyType& key,
    bool value) const {
  context_.SetBoolean(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetInteger(
    const KeyType& key,
    int value) const {
  context_.SetInteger(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetDouble(
    const KeyType& key,
    double value) const {
  context_.SetDouble(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetString(
    const KeyType& key,
    const std::string& value) const {
  context_.SetString(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetString(
    const KeyType& key,
    const base::string16& value) const {
  context_.SetString(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetStringList(
    const KeyType& key,
    const StringList& value) const {
  context_.SetStringList(key, value);
  return *this;
}

const BaseScreen::ContextEditor& BaseScreen::ContextEditor::SetString16List(
    const KeyType& key,
    const String16List& value) const {
  context_.SetString16List(key, value);
  return *this;
}

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

BaseScreen::ContextEditor BaseScreen::GetContextEditor() {
  return ContextEditor(*this);
}

void BaseScreen::OnContextChanged(const base::DictionaryValue& diff) {
  std::vector<::login::ScreenContext::KeyType> keys;
  context_.ApplyChanges(diff, &keys);
  for (const auto& key : keys)
    OnContextKeyUpdated(key);
}

}  // namespace chromeos
