// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/local_card_migration_dialog_controller_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"

// TODO(crbug.com/867194): Add time counter for showing 'close' button if
// uploading takes too long.

namespace autofill {

LocalCardMigrationDialogControllerImpl::LocalCardMigrationDialogControllerImpl()
    : local_card_migration_dialog_(nullptr) {}

LocalCardMigrationDialogControllerImpl::
    ~LocalCardMigrationDialogControllerImpl() {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_->CloseDialog();
}

void LocalCardMigrationDialogControllerImpl::ShowDialog(
    LocalCardMigrationDialog* local_card_migration_dialog,
    base::OnceClosure user_accepted_migration_closure) {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_->CloseDialog();

  local_card_migration_dialog_ = local_card_migration_dialog;
  local_card_migration_dialog_->ShowDialog(
      std::move(user_accepted_migration_closure));
}

LocalCardMigrationDialogState
LocalCardMigrationDialogControllerImpl::GetViewState() const {
  return view_state_;
}

void LocalCardMigrationDialogControllerImpl::SetViewState(
    LocalCardMigrationDialogState view_state) {
  view_state_ = view_state;
}

std::vector<MigratableCreditCard>&
LocalCardMigrationDialogControllerImpl::GetCardList() {
  return migratable_credit_cards_;
}

void LocalCardMigrationDialogControllerImpl::SetCardList(
    std::vector<MigratableCreditCard>& migratable_credit_cards) {
  migratable_credit_cards_ = migratable_credit_cards;
}

void LocalCardMigrationDialogControllerImpl::OnDialogClosed() {
  if (local_card_migration_dialog_)
    local_card_migration_dialog_ = nullptr;
}

}  // namespace autofill