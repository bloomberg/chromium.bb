// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller.h"

namespace chromeos {

// static
const int ManagedUserCreationController::kDummyAvatarIndex = -111;

ManagedUserCreationController::StatusConsumer::~StatusConsumer() {}

// static
ManagedUserCreationController*
    ManagedUserCreationController::current_controller_ = NULL;

ManagedUserCreationController::ManagedUserCreationController(
    ManagedUserCreationController::StatusConsumer* consumer)
    : consumer_(consumer) {
  DCHECK(!current_controller_) << "More than one controller exist.";
  current_controller_ = this;
}

ManagedUserCreationController::~ManagedUserCreationController() {
  current_controller_ = NULL;
}

}  // namespace chromeos

