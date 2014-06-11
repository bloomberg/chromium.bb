// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_icon.h"

ManagePasswordsIcon::ManagePasswordsIcon()
    : state_(password_manager::ui::INACTIVE_STATE),
      active_(false) {
}

ManagePasswordsIcon::~ManagePasswordsIcon() {
}

void ManagePasswordsIcon::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateVisibleUI();
}

void ManagePasswordsIcon::SetState(password_manager::ui::State state) {
  if (state_ == state)
    return;
  state_ = state;
  UpdateVisibleUI();
}
