// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/sync/driver/sync_service.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

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

uint64_t CalculateSyncPasswordHash(const base::StringPiece16& text,
                                   const std::string& salt) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  constexpr size_t kBytesFromHash = 8;
  constexpr uint64_t kScryptCost = 32;  // It must be power of 2.
  constexpr uint64_t kScryptBlockSize = 8;
  constexpr uint64_t kScryptParallelization = 1;
  constexpr size_t kScryptMaxMemory = 1024 * 1024;

  uint8_t hash[kBytesFromHash];
  base::StringPiece text_8bits(reinterpret_cast<const char*>(text.data()),
                               text.size() * 2);
  const uint8_t* salt_ptr = reinterpret_cast<const uint8_t*>(salt.c_str());

  int scrypt_ok = EVP_PBE_scrypt(text_8bits.data(), text_8bits.size(), salt_ptr,
                                 salt.size(), kScryptCost, kScryptBlockSize,
                                 kScryptParallelization, kScryptMaxMemory, hash,
                                 kBytesFromHash);

  // EVP_PBE_scrypt can only fail due to memory allocation error (which aborts
  // Chromium) or invalid parameters. In case of a failure a hash could leak
  // information from the stack, so using CHECK is better than DCHECK.
  CHECK(scrypt_ok);

  // Take 37 bits of |hash|.
  uint64_t hash37 = ((static_cast<uint64_t>(hash[0]))) |
                    ((static_cast<uint64_t>(hash[1])) << 8) |
                    ((static_cast<uint64_t>(hash[2])) << 16) |
                    ((static_cast<uint64_t>(hash[3])) << 24) |
                    (((static_cast<uint64_t>(hash[4])) & 0x1F) << 32);

  return hash37;
}

}  // namespace password_manager_util
