// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace {

constexpr char kJsScreenPath[] = "login.EncryptionMigrationScreen";

// JS API callbacks names.
constexpr char kJsApiStartMigration[] = "startMigration";
constexpr char kJsApiRequestRestart[] = "requestRestart";

}  // namespace

namespace chromeos {

EncryptionMigrationScreenHandler::EncryptionMigrationScreenHandler()
    : BaseScreenHandler(kScreenId), weak_ptr_factory_(this) {
  set_call_js_prefix(kJsScreenPath);
}

EncryptionMigrationScreenHandler::~EncryptionMigrationScreenHandler() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void EncryptionMigrationScreenHandler::Show() {
  if (!page_is_ready() || !delegate_) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void EncryptionMigrationScreenHandler::Hide() {
  show_on_init_ = false;
}

void EncryptionMigrationScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void EncryptionMigrationScreenHandler::SetUserContext(
    const UserContext& user_context) {
  user_context_ = user_context;
}

void EncryptionMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void EncryptionMigrationScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EncryptionMigrationScreenHandler::RegisterMessages() {
  AddCallback(kJsApiStartMigration,
              &EncryptionMigrationScreenHandler::HandleStartMigration);
  AddCallback(kJsApiRequestRestart,
              &EncryptionMigrationScreenHandler::HandleRequestRestart);
}

void EncryptionMigrationScreenHandler::HandleStartMigration() {
  StartMigration();
}

void EncryptionMigrationScreenHandler::HandleRequestRestart() {
  // TODO(fukino): If the migration finished successfully, we don't need to
  // restart the device. Let's sign in to the desktop using the already-provided
  // user credential.
  chrome::AttemptRestart();
}

void EncryptionMigrationScreenHandler::UpdateUIState(UIState state) {
  if (state == current_ui_state_)
    return;

  current_ui_state_ = state;
  CallJS("setUIState", static_cast<int>(state));
}

void EncryptionMigrationScreenHandler::StartMigration() {
  DBusThreadManager::Get()
      ->GetCryptohomeClient()
      ->SetDircryptoMigrationProgressHandler(
          base::Bind(&EncryptionMigrationScreenHandler::OnMigrationProgress,
                     weak_ptr_factory_.GetWeakPtr()));

  // |auth_key| is created in the same manner as CryptohomeAuthenticator.
  const Key* key = user_context_.GetKey();
  // If the |key| is a plain text password, crash rather than attempting to
  // mount the cryptohome with a plain text password.
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  // Set the authentication's key label to an empty string, which is a wildcard
  // allowing any key to match. This is necessary because cryptohomes created by
  // Chrome OS M38 and older will have a legacy key with no label while those
  // created by Chrome OS M39 and newer will have a key with the label
  // kCryptohomeGAIAKeyLabel.
  const cryptohome::KeyDefinition auth_key(key->GetSecret(), std::string(),
                                           cryptohome::PRIV_DEFAULT);
  cryptohome::HomedirMethods::GetInstance()->MigrateToDircrypto(
      cryptohome::Identification(user_context_.GetAccountId()),
      cryptohome::Authorization(auth_key),
      base::Bind(&EncryptionMigrationScreenHandler::OnMigrationRequested,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnMigrationProgress(
    cryptohome::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  switch (status) {
    case cryptohome::DIRCRYPTO_MIGRATION_INITIALIZING:
      UpdateUIState(UIState::MIGRATING);
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_IN_PROGRESS:
      UpdateUIState(UIState::MIGRATING);
      CallJS("setMigrationProgress", static_cast<double>(current) / total);
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_SUCCESS:
    case cryptohome::DIRCRYPTO_MIGRATION_FAILED:
      UpdateUIState(status == cryptohome::DIRCRYPTO_MIGRATION_SUCCESS
                        ? UIState::MIGRATION_SUCCEEDED
                        : UIState::MIGRATION_FAILED);
      // Stop listening to the progress updates.
      DBusThreadManager::Get()
          ->GetCryptohomeClient()
          ->SetDircryptoMigrationProgressHandler(
              CryptohomeClient::DircryptoMigrationProgessHandler());
      break;
    default:
      break;
  }
}

void EncryptionMigrationScreenHandler::OnMigrationRequested(bool success) {
  // This function is called when MigrateToDircrypto is correctly requested.
  // It does not mean that the migration is completed. We should know the
  // completion by DircryptoMigrationProgressHandler. success == false means a
  // failure in DBus communication.
  // TODO(fukino): Handle this case. Should we retry or restart?
  DCHECK(success);
}

}  // namespace chromeos
