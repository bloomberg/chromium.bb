// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_window.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

// Global lock screen instance. There can only ever be on lock screen at a
// time.
LockScreen* instance_ = nullptr;

}  // namespace

LockScreen::LockScreen() = default;

LockScreen::~LockScreen() = default;

// static
LockScreen* LockScreen::Get() {
  CHECK(instance_);
  return instance_;
}

// static
void LockScreen::Show() {
  CHECK(!instance_);
  instance_ = new LockScreen();

  auto data_dispatcher = base::MakeUnique<LoginDataDispatcher>();
  auto* contents = new LockContentsView(data_dispatcher.get());

  // TODO(jdufault|crbug.com/731191): Call NotifyUsers via
  // LockScreenController::LoadUsers once it uses a mojom specific type.
  std::vector<mojom::UserInfoPtr> users;
  for (const mojom::UserSessionPtr& session :
       Shell::Get()->session_controller()->GetUserSessions()) {
    users.push_back(session->user_info->Clone());
  }
  data_dispatcher->NotifyUsers(users);

  auto* window = instance_->window_ = new LockWindow();
  window->SetBounds(display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  window->SetContentsView(contents);
  window->set_data_dispatcher(std::move(data_dispatcher));
  window->Show();

  // TODO(jdufault): Use correct blur amount.
  window->GetLayer()->SetBackgroundBlur(20);
}

void LockScreen::Destroy() {
  CHECK_EQ(instance_, this);
  window_->Close();
  delete instance_;
  instance_ = nullptr;
}

void LockScreen::SetPinEnabledForUser(const AccountId& account_id,
                                      bool is_enabled) {
  window_->data_dispatcher()->SetPinEnabledForUser(account_id, is_enabled);
}

}  // namespace ash
