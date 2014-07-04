// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/screen_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/screen_context.h"
#include "chrome/browser/chromeos/login/screens/screen_factory.h"
#include "chrome/browser/chromeos/login/screens/screen_flow.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"

namespace chromeos {

ScreenManager::ScreenManager(ScreenFactory* factory,
                             OobeDisplay* oobe_display,
                             ScreenFlow* initial_flow)
    : factory_(factory),
      display_(oobe_display),
      flow_(initial_flow),
      weak_factory_(this) {
  js_is_ready_ = display_->IsJSReady(
      base::Bind(&ScreenManager::OnDisplayIsReady, weak_factory_.GetWeakPtr()));

  // TODO({antrim|ygorshenin}@): register ScreenManager as delegate in
  // ScreenManagerHandler instance.
}

ScreenManager::~ScreenManager() {
}

void ScreenManager::WarmupScreen(const std::string& id,
                                 ScreenContext* context) {

}

void ScreenManager::ShowScreen(const std::string& id) {
  ShowScreenImpl(id, new ScreenContext(), false);
}

void ScreenManager::ShowScreenWithParameters(const std::string& id,
                                             ScreenContext* context) {
  ShowScreenImpl(id, context, false);
}

void ScreenManager::PopupScreen(const std::string& id) {
  ShowScreenImpl(id, new ScreenContext(), true);
}

void ScreenManager::PopupScreenWithParameters(const std::string& id,
                                              ScreenContext* context) {
  ShowScreenImpl(id, context, true);
}

void ScreenManager::HidePopupScreen(const std::string& screen_id) {
  DCHECK(!screen_stack_.empty());
  DCHECK_EQ(screen_stack_.top(), screen_id);

  // TODO(antrim): check that id really exist in stack.
  std::string previous_screen = GetCurrentScreenId();
  GetTopmostScreen()->OnHide();
  TearDownTopmostScreen();
  screen_stack_.pop();

  std::string screen_below;
  if (!screen_stack_.empty()) {
    screen_below = GetCurrentScreenId();
    GetTopmostScreen()->OnShow();
  }
  TransitionScreen(previous_screen, screen_below);
}

std::string ScreenManager::GetCurrentScreenId() {
  DCHECK(!screen_stack_.empty());
  return screen_stack_.top();
}

void ScreenManager::SetScreenFlow(ScreenFlow* flow) {
  if (flow)
    flow->set_screen_manager(this);
  // TODO(antrim): delayed reset.
  flow_.reset(flow);
}

void ScreenManager::ShowScreenImpl(const std::string& id,
                                   ScreenContext* context,
                                   bool popup) {
  if (!js_is_ready_) {
    DCHECK(!start_screen_params_.get());
    start_screen_params_.reset(context);
    start_screen_popup_ = popup;
    start_screen_ = id;
    return;
  }
  DCHECK(popup || screen_stack_.size() <= 1);
  BaseScreen* screen = FindOrCreateScreen(id);
  screen->SetContext(context);
  screen->Initialize(context);

  std::string previous_screen;
  if (!screen_stack_.empty()) {
    previous_screen = GetCurrentScreenId();
    GetTopmostScreen()->OnHide();
    if (!popup)
      TearDownTopmostScreen();
  }

  DCHECK(popup || screen_stack_.empty());
  screen_stack_.push(id);
  screen->OnShow();
  TransitionScreen(previous_screen, id);
}

void ScreenManager::TransitionScreen(const std::string& from_id,
                                     const std::string& to_id) {
}

void ScreenManager::TearDownTopmostScreen() {
  DCHECK(!screen_stack_.empty());
  std::string screen_id = screen_stack_.top();

  ScreenMap::iterator i = existing_screens_.find(screen_id);

  if (i == existing_screens_.end()) {
    NOTREACHED();
    return;
  }
  BaseScreen* screen = i->second.get();
  if (!screen->IsPermanent()) {
    screen->OnClose();
    existing_screens_.erase(i);
  }
}

void ScreenManager::OnDisplayIsReady() {
  js_is_ready_ = true;
  ShowScreenImpl(
      start_screen_,
      start_screen_params_.release(),
      start_screen_popup_);
}

BaseScreen* ScreenManager::GetTopmostScreen() {
  DCHECK(!screen_stack_.empty());
  return FindOrCreateScreen(screen_stack_.top());
}

BaseScreen* ScreenManager::FindOrCreateScreen(const std::string& id) {
  ScreenMap::iterator i = existing_screens_.find(id);
  if (i != existing_screens_.end())
    return i->second.get();
  BaseScreen* result = factory_->CreateScreen(id);
  existing_screens_[id] = linked_ptr<BaseScreen>(result);
  return result;
}

void ScreenManager::OnButtonPressed(const std::string& screen_name,
                                    const std::string& button_id) {
  CallOnScreen<const std::string&>(screen_name,
                                   &BaseScreen::OnButtonPressed,
                                   button_id);
}

void ScreenManager::OnContextChanged(const std::string& screen_name,
                                     const base::DictionaryValue* diff) {
  CallOnScreen<const base::DictionaryValue*>(screen_name,
                                             &BaseScreen::OnContextChanged,
                                             diff);
}

}  // namespace chromeos
