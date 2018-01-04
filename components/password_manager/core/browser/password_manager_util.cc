// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <algorithm>

#include "base/stl_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/sync_service.h"

namespace password_manager_util {

password_manager::PasswordSyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service) {
  if (sync_service && sync_service->IsFirstSetupComplete() &&
      sync_service->IsSyncActive() &&
      sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    return sync_service->IsUsingSecondaryPassphrase()
               ? password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE
               : password_manager::SYNCING_NORMAL_ENCRYPTION;
  }
  return password_manager::NOT_SYNCING_PASSWORDS;
}

void FindDuplicates(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* duplicates,
    std::vector<std::vector<autofill::PasswordForm*>>* tag_groups) {
  if (forms->empty())
    return;

  // Linux backends used to treat the first form as a prime oneamong the
  // duplicates. Therefore, the caller should try to preserve it.
  std::stable_sort(forms->begin(), forms->end(), autofill::LessThanUniqueKey());

  std::vector<std::unique_ptr<autofill::PasswordForm>> unique_forms;
  unique_forms.push_back(std::move(forms->front()));
  if (tag_groups) {
    tag_groups->clear();
    tag_groups->push_back(std::vector<autofill::PasswordForm*>());
    tag_groups->front().push_back(unique_forms.front().get());
  }
  for (auto it = forms->begin() + 1; it != forms->end(); ++it) {
    if (ArePasswordFormUniqueKeyEqual(**it, *unique_forms.back())) {
      if (tag_groups)
        tag_groups->back().push_back(it->get());
      duplicates->push_back(std::move(*it));
    } else {
      if (tag_groups)
        tag_groups->push_back(
            std::vector<autofill::PasswordForm*>(1, it->get()));
      unique_forms.push_back(std::move(*it));
    }
  }
  forms->swap(unique_forms);
}

void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* android_credentials) {
  // Remove username-only credentials which are not federated.
  base::EraseIf(*android_credentials,
                [](const std::unique_ptr<autofill::PasswordForm>& form) {
                  return form->scheme ==
                             autofill::PasswordForm::SCHEME_USERNAME_ONLY &&
                         form->federation_origin.unique();
                });

  // Set "skip_zero_click" on federated credentials.
  std::for_each(
      android_credentials->begin(), android_credentials->end(),
      [](const std::unique_ptr<autofill::PasswordForm>& form) {
        if (form->scheme == autofill::PasswordForm::SCHEME_USERNAME_ONLY)
          form->skip_zero_click = true;
      });
}

bool IsLoggingActive(const password_manager::PasswordManagerClient* client) {
  const password_manager::LogManager* log_manager = client->GetLogManager();
  return log_manager && log_manager->IsLoggingActive();
}

bool ManualPasswordGenerationEnabled(syncer::SyncService* sync_service) {
  if (!(base::FeatureList::IsEnabled(
            password_manager::features::kEnableManualPasswordGeneration) &&
        (password_manager_util::GetPasswordSyncState(sync_service) ==
         password_manager::SYNCING_NORMAL_ENCRYPTION))) {
    return false;
  }
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_SHOWN);
  return true;
}

bool ShowAllSavedPasswordsContextMenuEnabled() {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kEnableShowAllSavedPasswordsContextMenu)) {
    return false;
  }
  LogContextOfShowAllSavedPasswordsShown(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);
  return true;
}

void UserTriggeredShowAllSavedPasswordsFromContextMenu(
    autofill::AutofillClient* autofill_client) {
  autofill_client->ExecuteCommand(
      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY);
  password_manager::metrics_util::LogContextOfShowAllSavedPasswordsAccepted(
      password_manager::metrics_util::
          SHOW_ALL_SAVED_PASSWORDS_CONTEXT_CONTEXT_MENU);
}

void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client) {
  password_manager_client->GeneratePassword();
  LogPasswordGenerationEvent(
      autofill::password_generation::PASSWORD_GENERATION_CONTEXT_MENU_PRESSED);
}

}  // namespace password_manager_util
