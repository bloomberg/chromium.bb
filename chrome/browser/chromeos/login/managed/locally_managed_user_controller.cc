// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_controller.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/mount_manager.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

LocallyManagedUserController::StatusConsumer::~StatusConsumer() {}

LocallyManagedUserController::UserCreationContext::UserCreationContext()
    : id_acquired(false), token_acquired(false) {}

LocallyManagedUserController::UserCreationContext::~UserCreationContext() {}

// static
LocallyManagedUserController*
    LocallyManagedUserController::current_controller_ = NULL;

LocallyManagedUserController::LocallyManagedUserController(
    LocallyManagedUserController::StatusConsumer* consumer)
    : consumer_(consumer),
      weak_factory_(this) {
  connector_.reset(new CloudConnector(this));
  if (current_controller_)
    NOTREACHED() << "More than one controller exist.";
  current_controller_ = this;
}

LocallyManagedUserController::~LocallyManagedUserController() {
  current_controller_ = NULL;
}

void LocallyManagedUserController::SetUpCreation(string16 display_name,
                                                 std::string password) {
  creation_context_.reset(
      new LocallyManagedUserController::UserCreationContext());
  creation_context_->display_name = display_name;
  creation_context_->password = password;
}

void LocallyManagedUserController::StartCreation() {
  DCHECK(creation_context_);
  UserManager::Get()->StartLocallyManagedUserCreationTransaction(
      creation_context_->display_name);
  connector_->GenerateNewUserId();
}

void LocallyManagedUserController::RetryLastStep() {
  DCHECK(creation_context_);
  if (!creation_context_->id_acquired) {
    connector_->GenerateNewUserId();
    return;
  }
  if (!creation_context_->token_acquired) {
    connector_->FetchDMToken(creation_context_->user_id);
    return;
  }
  NOTREACHED();
}

void LocallyManagedUserController::FinishCreation() {
  chrome::AttemptUserExit();
}

// CloudConnector::Delegate overrides
void LocallyManagedUserController::NewUserIdGenerated(std::string& new_id) {
  DCHECK(creation_context_.get());
  const User* user = UserManager::Get()->CreateLocallyManagedUserRecord(
      new_id, creation_context_->display_name);

  creation_context_->id_acquired = true;
  creation_context_->user_id = user->email();

  UserManager::Get()->SetLocallyManagedUserCreationTransactionUserId(
      creation_context_->user_id);

  authenticator_ = new ManagedUserAuthenticator(this);
  authenticator_->AuthenticateToCreate(user->email(),
                                       creation_context_->password);
}

void LocallyManagedUserController::OnCloudError(
    CloudConnector::CloudError error) {
  ErrorCode code = NO_ERROR;
  switch (error) {
    case CloudConnector::NOT_CONNECTED:
      code = CLOUD_NOT_CONNECTED;
      break;
    case CloudConnector::TIMED_OUT:
      code = CLOUD_TIMED_OUT;
      break;
    case CloudConnector::SERVER_ERROR:
      code = CLOUD_SERVER_ERROR;
      break;
    default:
      NOTREACHED();
  }

  if (consumer_)
    consumer_->OnCreationError(code, true);
}

void LocallyManagedUserController::OnAuthenticationFailure(
    ManagedUserAuthenticator::AuthState error) {
  ErrorCode code = NO_ERROR;
  switch (error) {
    case ManagedUserAuthenticator::NO_MOUNT:
      code = CRYPTOHOME_NO_MOUNT;
      break;
    case ManagedUserAuthenticator::FAILED_MOUNT:
      code = CRYPTOHOME_FAILED_MOUNT;
      break;
    case ManagedUserAuthenticator::FAILED_TPM:
      code = CRYPTOHOME_FAILED_TPM;
      break;
    default:
      NOTREACHED();
  }
  if (consumer_)
    consumer_->OnCreationError(code, false);
}

void LocallyManagedUserController::OnMountSuccess(
    const std::string& mount_hash) {
  creation_context_->mount_hash = mount_hash;
  connector_->FetchDMToken(creation_context_->user_id);
}

void LocallyManagedUserController::OnCreationSuccess() {
  connector_->FetchDMToken(creation_context_->user_id);
}

void LocallyManagedUserController::DMTokenFetched(std::string& user_id,
                                                  std::string& token) {
  creation_context_->token_acquired = true;
  creation_context_->token = token;

  content::BrowserThread::GetBlockingPool()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&LocallyManagedUserController::StoreManagedUserFiles,
           weak_factory_.GetWeakPtr(),
           MountManager::GetHomeDir(creation_context_->mount_hash)),
      base::Bind(&LocallyManagedUserController::OnManagedUserFilesStored,
           weak_factory_.GetWeakPtr()));
}

void LocallyManagedUserController::StoreManagedUserFiles(
    const base::FilePath& base_path) {
  base::FilePath token_file = base_path.Append("token");

  file_util::WriteFile(token_file,
      creation_context_->token.c_str(),
      creation_context_->token.length());
}

void LocallyManagedUserController::OnManagedUserFilesStored() {
  UserManager::Get()->CommitLocallyManagedUserCreationTransaction();
  if (consumer_)
    consumer_->OnCreationSuccess();
}

}  // namespace chromeos
