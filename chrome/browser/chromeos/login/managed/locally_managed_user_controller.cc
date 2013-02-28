// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_controller.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

// static
LocallyManagedUserController*
    LocallyManagedUserController::current_controller_ = NULL;

LocallyManagedUserController::LocallyManagedUserController() {
  connector_.reset(new CloudConnector(this));
  if (current_controller_)
    NOTREACHED() << "More than one controller exist.";
  current_controller_ = this;
}

LocallyManagedUserController::~LocallyManagedUserController() {
  current_controller_ = NULL;
}

void LocallyManagedUserController::StartCreation(string16 display_name,
                                                 std::string password) {
  // Start transaction
  UserManager::Get()->StartLocallyManagedUserCreationTransaction(display_name);
  creation_context_.reset(
      new LocallyManagedUserController::UserCreationContext());
  creation_context_->display_name = display_name;
  connector_->GenerateNewUserId();
}

void LocallyManagedUserController::FinishCreation() {
  UserManager::Get()->CommitLocallyManagedUserCreationTransaction();
  chrome::AttemptUserExit();
}

// CloudConnector::Delegate overrides
void LocallyManagedUserController::NewUserIdGenerated(std::string new_id) {
  DCHECK(creation_context_.get());
  const User* user = UserManager::Get()->CreateLocallyManagedUserRecord(new_id,
      creation_context_->display_name);

  UserManager::Get()->SetLocallyManagedUserCreationTransactionUserId(
      user->email());
}

void LocallyManagedUserController::OnCloudError(
    CloudConnector::CloudError error) {
  // TODO(antrim): add ui for error handling.
  NOTREACHED();
}

}  // namespace chromeos
