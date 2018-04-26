// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"

#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_cryptohome.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"

namespace chromeos {
namespace quick_unlock {

namespace {

PinStorageCryptohome* GetCryptohomeStorage() {
  static base::NoDestructor<PinStorageCryptohome> instance;
  return instance.get();
}

QuickUnlockStorage* GetPrefsBackend(const AccountId& account_id) {
  return QuickUnlockFactory::GetForAccountId(account_id);
}

void PostResponse(PinBackend::BoolCallback result, bool value) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result), value));
}

}  // namespace

// static
void PinBackend::IsSet(const AccountId& account_id, BoolCallback result) {
  if (GetPinStorageType() == PinStorageType::kCryptohome) {
    GetCryptohomeStorage()->IsPinSetInCryptohome(account_id, std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    PostResponse(std::move(result),
                 storage && storage->pin_storage_prefs()->IsPinSet());
  }
}

// static
void PinBackend::Set(const AccountId& account_id,
                     const std::string& token,
                     const std::string& pin,
                     BoolCallback did_set) {
  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  DCHECK(storage);

  if (GetPinStorageType() == PinStorageType::kCryptohome) {
    // If |user_context| is null, then the token timed out.
    UserContext* user_context = storage->GetUserContext(token);
    if (!user_context) {
      PostResponse(std::move(did_set), false);
      return;
    }
    // There may be a pref value if resetting PIN and the device now supports
    // cryptohome-based PIN.
    storage->pin_storage_prefs()->RemovePin();
    GetCryptohomeStorage()->SetPin(*user_context, pin, std::move(did_set));
  } else {
    storage->pin_storage_prefs()->SetPin(pin);
    storage->MarkStrongAuth();
    PostResponse(std::move(did_set), true);
  }
}

// static
void PinBackend::Remove(const AccountId& account_id,
                        const std::string& token,
                        BoolCallback did_remove) {
  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  DCHECK(storage);

  if (GetPinStorageType() == PinStorageType::kCryptohome) {
    // If |user_context| is null, then the token timed out.
    UserContext* user_context = storage->GetUserContext(token);
    if (!user_context) {
      PostResponse(std::move(did_remove), false);
      return;
    }
    GetCryptohomeStorage()->RemovePin(*user_context, std::move(did_remove));
  } else {
    const bool had_pin = storage->pin_storage_prefs()->IsPinSet();
    storage->pin_storage_prefs()->RemovePin();
    PostResponse(std::move(did_remove), had_pin);
  }
}

// static
void PinBackend::CanAuthenticate(const AccountId& account_id,
                                 BoolCallback result) {
  if (GetPinStorageType() == PinStorageType::kCryptohome) {
    GetCryptohomeStorage()->IsPinSetInCryptohome(account_id, std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    PostResponse(
        std::move(result),
        storage && storage->HasStrongAuth() &&
            storage->pin_storage_prefs()->IsPinAuthenticationAvailable());
  }
}

// static
void PinBackend::TryAuthenticate(const AccountId& account_id,
                                 const std::string& key,
                                 const Key::KeyType& key_type,
                                 BoolCallback result) {
  if (GetPinStorageType() == PinStorageType::kCryptohome) {
    GetCryptohomeStorage()->TryAuthenticate(account_id, key, key_type,
                                            std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    DCHECK(storage);

    if (!storage->HasStrongAuth()) {
      PostResponse(std::move(result), false);
    } else {
      PostResponse(
          std::move(result),
          storage->pin_storage_prefs()->TryAuthenticatePin(key, key_type));
    }
  }
}

// static
std::string PinBackend::ComputeSecret(const std::string& pin,
                                      const std::string& salt,
                                      Key::KeyType key_type) {
  DCHECK(!pin.empty());
  DCHECK(!salt.empty());
  if (key_type != Key::KEY_TYPE_PASSWORD_PLAIN)
    return pin;

  Key key(pin);
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, salt);
  return key.GetSecret();
}

// static
void PinBackend::ResetForTesting() {
  new (GetCryptohomeStorage()) PinStorageCryptohome();
}

}  // namespace quick_unlock
}  // namespace chromeos
