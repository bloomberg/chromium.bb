// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_window.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
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

  auto* contents = new LockContentsView();
  auto* window = instance_->window_ = new LockWindow();
  window->SetBounds(display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  window->SetContentsView(contents);
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
  NOTIMPLEMENTED();
}

}  // namespace ash
