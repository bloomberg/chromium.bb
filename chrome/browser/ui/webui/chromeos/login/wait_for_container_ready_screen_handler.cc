// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/wait_for_container_ready_screen_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/wait_for_container_ready_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

constexpr char kJsScreenPath[] = "login.WaitForContainerReadyScreen";
constexpr base::TimeDelta kWaitingTimeout = base::TimeDelta::FromMinutes(1);

}  // namespace

namespace chromeos {

WaitForContainerReadyScreenHandler::WaitForContainerReadyScreenHandler()
    : BaseScreenHandler(kScreenId), weak_ptr_factory_(this) {
  set_call_js_prefix(kJsScreenPath);
  arc::ArcSessionManager::Get()->AddObserver(this);
  is_container_ready_ = arc::ArcSessionManager::Get()->IsSessionRunning();
}

WaitForContainerReadyScreenHandler::~WaitForContainerReadyScreenHandler() {
  if (screen_) {
    screen_->OnViewDestroyed(this);
  }
  timer_.Stop();
  if (arc::ArcSessionManager::Get())
    arc::ArcSessionManager::Get()->RemoveObserver(this);
}

void WaitForContainerReadyScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("waitForContainerReadyTitle",
               IDS_WAIT_FOR_CONTAINER_READY_TITLE);
  builder->Add("waitForContainerReadyIntroMessage",
               IDS_WAIT_FOR_CONTAINER_READY_INTRO_MESSAGE);
}

void WaitForContainerReadyScreenHandler::Bind(
    WaitForContainerReadyScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
  if (page_is_ready())
    Initialize();
}

void WaitForContainerReadyScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
  timer_.Stop();
}

void WaitForContainerReadyScreenHandler::Show() {
  if (!page_is_ready() || !screen_) {
    show_on_init_ = true;
    return;
  }

  if (is_container_ready_) {
    NotifyContainerReady();
    return;
  }

  timer_.Start(
      FROM_HERE, kWaitingTimeout,
      base::Bind(&WaitForContainerReadyScreenHandler::OnMaxContainerWaitTimeout,
                 weak_ptr_factory_.GetWeakPtr()));

  ShowScreen(kScreenId);
}

void WaitForContainerReadyScreenHandler::Hide() {}

void WaitForContainerReadyScreenHandler::OnArcInitialStart() {
  is_container_ready_ = true;
  if (!screen_)
    return;

  // TODO(updowndota): Remove the temporary delay after the potential racing
  // issue is eliminated.
  timer_.Stop();
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(5),
      base::Bind(&WaitForContainerReadyScreenHandler::NotifyContainerReady,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WaitForContainerReadyScreenHandler::Initialize() {
  if (!screen_ || !show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void WaitForContainerReadyScreenHandler::OnMaxContainerWaitTimeout() {
  // TODO(updowndota): Add histogram to Voice Interaction OptIn flow.
  if (screen_)
    screen_->OnContainerReady();
}

void WaitForContainerReadyScreenHandler::NotifyContainerReady() {
  if (screen_)
    screen_->OnContainerReady();
}

}  // namespace chromeos
