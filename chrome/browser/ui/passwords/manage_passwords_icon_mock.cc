// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_icon_mock.h"

ManagePasswordsIconMock::ManagePasswordsIconMock() : bubble_shown_count_(0) {
}

ManagePasswordsIconMock::~ManagePasswordsIconMock() {
}

void ManagePasswordsIconMock::ShowBubbleWithoutUserInteraction() {
  ++bubble_shown_count_;
}

void ManagePasswordsIconMock::SetStateInternal(ManagePasswordsIcon::State) {
}
