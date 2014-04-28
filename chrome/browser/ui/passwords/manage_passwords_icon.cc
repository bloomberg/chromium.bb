// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_icon.h"

ManagePasswordsIcon::ManagePasswordsIcon() : state_(INACTIVE_STATE) {
}

ManagePasswordsIcon::~ManagePasswordsIcon() {
}

void ManagePasswordsIcon::SetState(State state) {
  if (state_ == state)
    return;
  state_ = state;
  SetStateInternal(state);
}
