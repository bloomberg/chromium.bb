// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"

namespace {

constexpr char kJsScreenPath[] = "login.EncryptionMigrationScreen";

}  // namespace

namespace chromeos {

EncryptionMigrationScreenHandler::EncryptionMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {
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

}  // namespace chromeos
