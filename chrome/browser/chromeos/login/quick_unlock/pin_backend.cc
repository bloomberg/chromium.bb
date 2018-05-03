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
#include "components/account_id/account_id.h"

namespace chromeos {
namespace quick_unlock {

namespace {

QuickUnlockStorage* GetPrefsBackend(const AccountId& account_id) {
  return QuickUnlockFactory::GetForAccountId(account_id);
}

void PostResponse(PinBackend::BoolCallback result, bool value) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result), value));
}

}  // namespace

// static
PinBackend* PinBackend::GetInstance() {
  static base::NoDestructor<PinBackend> instance;
  return instance.get();
}

// static
void PinBackend::ResetForTesting() {
  new (GetInstance()) PinStorageCryptohome();
}

PinBackend::PinBackend() {
  // Always use prefs backend.
  // TODO(jdufault): Add support for cryptohome backend.
  resolving_backend_ = false;
}

PinBackend::~PinBackend() {
  DCHECK(on_cryptohome_support_received_.empty());
}

void PinBackend::IsSet(const AccountId& account_id, BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::IsSet, base::Unretained(this), account_id,
                       std::move(result)));
    return;
  }

  if (cryptohome_backend_) {
    cryptohome_backend_->IsPinSetInCryptohome(account_id, std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    PostResponse(std::move(result),
                 storage && storage->pin_storage_prefs()->IsPinSet());
  }
}

void PinBackend::Set(const AccountId& account_id,
                     const std::string& token,
                     const std::string& pin,
                     BoolCallback did_set) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::Set, base::Unretained(this), account_id,
                       token, pin, std::move(did_set)));
    return;
  }

  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  DCHECK(storage);

  if (cryptohome_backend_) {
    // If |user_context| is null, then the token timed out.
    UserContext* user_context = storage->GetUserContext(token);
    if (!user_context) {
      PostResponse(std::move(did_set), false);
      return;
    }
    // There may be a pref value if resetting PIN and the device now supports
    // cryptohome-based PIN.
    storage->pin_storage_prefs()->RemovePin();
    cryptohome_backend_->SetPin(*user_context, pin, std::move(did_set));
  } else {
    storage->pin_storage_prefs()->SetPin(pin);
    storage->MarkStrongAuth();
    PostResponse(std::move(did_set), true);
  }
}

void PinBackend::Remove(const AccountId& account_id,
                        const std::string& token,
                        BoolCallback did_remove) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::Remove, base::Unretained(this), account_id,
                       token, std::move(did_remove)));
    return;
  }

  QuickUnlockStorage* storage = GetPrefsBackend(account_id);
  DCHECK(storage);

  if (cryptohome_backend_) {
    // If |user_context| is null, then the token timed out.
    UserContext* user_context = storage->GetUserContext(token);
    if (!user_context) {
      PostResponse(std::move(did_remove), false);
      return;
    }
    cryptohome_backend_->RemovePin(*user_context, std::move(did_remove));
  } else {
    const bool had_pin = storage->pin_storage_prefs()->IsPinSet();
    storage->pin_storage_prefs()->RemovePin();
    PostResponse(std::move(did_remove), had_pin);
  }
}

void PinBackend::CanAuthenticate(const AccountId& account_id,
                                 BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::CanAuthenticate, base::Unretained(this),
                       account_id, std::move(result)));
    return;
  }

  if (cryptohome_backend_) {
    cryptohome_backend_->IsPinSetInCryptohome(account_id, std::move(result));
  } else {
    QuickUnlockStorage* storage = GetPrefsBackend(account_id);
    PostResponse(
        std::move(result),
        storage && storage->HasStrongAuth() &&
            storage->pin_storage_prefs()->IsPinAuthenticationAvailable());
  }
}

void PinBackend::TryAuthenticate(const AccountId& account_id,
                                 const std::string& key,
                                 const Key::KeyType& key_type,
                                 BoolCallback result) {
  if (resolving_backend_) {
    on_cryptohome_support_received_.push_back(
        base::BindOnce(&PinBackend::TryAuthenticate, base::Unretained(this),
                       account_id, key, key_type, std::move(result)));
    return;
  }

  if (cryptohome_backend_) {
    cryptohome_backend_->TryAuthenticate(account_id, key, key_type,
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

}  // namespace quick_unlock
}  // namespace chromeos
