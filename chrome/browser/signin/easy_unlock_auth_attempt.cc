// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_auth_attempt.h"

#include "base/logging.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"


#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#endif

namespace {

// Fake secret used to force invalid login.
const char kStubSecret[] = "\xFF\x00";

// Decrypts the secret that should be used to login from |wrapped_secret| using
// raw AES key |raw_key|.
// In a case of error, an empty string is returned.
std::string UnwrapSecret(const std::string& wrapped_secret,
                         const std::string& raw_key) {
  if (raw_key.empty())
    return std::string();

  // Import the key structure.
  scoped_ptr<crypto::SymmetricKey> key(
     crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key));

  if (!key)
    return std::string();

  std::string iv(raw_key.size(), ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CBC, iv))
    return std::string();

  std::string secret;
  if (!encryptor.Decrypt(wrapped_secret, &secret))
    return std::string();

  return secret;
}

}  // namespace

EasyUnlockAuthAttempt::EasyUnlockAuthAttempt(Profile* profile,
                                             const std::string& user_id,
                                             Type type)
    : profile_(profile),
      state_(STATE_IDLE),
      user_id_(user_id),
      type_(type) {
}

EasyUnlockAuthAttempt::~EasyUnlockAuthAttempt() {
  if (state_ == STATE_RUNNING)
    Cancel(user_id_);
}

bool EasyUnlockAuthAttempt::Start(const std::string& user_id) {
  DCHECK(state_ == STATE_IDLE);

  if (!ScreenlockBridge::Get()->IsLocked())
    return false;

  if (user_id != user_id_) {
    Cancel(user_id);
    return false;
  }

  ScreenlockBridge::LockHandler::AuthType auth_type =
      ScreenlockBridge::Get()->lock_handler()->GetAuthType(user_id);

  if (auth_type != ScreenlockBridge::LockHandler::USER_CLICK) {
    Cancel(user_id);
    return false;
  }

  state_ = STATE_RUNNING;

  // TODO(tbarzic): Replace this with an easyUnlockPrivate event that will
  // report more context to the app (e.g. user id, whether the attempt is for
  // signin or unlock).
  extensions::ScreenlockPrivateEventRouter* router =
      extensions::ScreenlockPrivateEventRouter::GetFactoryInstance()->Get(
          profile_);
  return router->OnAuthAttempted(auth_type, "");
}

void EasyUnlockAuthAttempt::FinalizeUnlock(const std::string& user_id,
                                           bool success) {
  if (state_ != STATE_RUNNING || user_id != user_id_)
    return;

  if (type_ != TYPE_UNLOCK) {
    Cancel(user_id_);
    return;
  }

  if (!ScreenlockBridge::Get()->IsLocked())
    return;

  if (success) {
    ScreenlockBridge::Get()->lock_handler()->Unlock(user_id_);
  } else {
    ScreenlockBridge::Get()->lock_handler()->EnableInput();
  }

  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::FinalizeSignin(const std::string& user_id,
                                           const std::string& wrapped_secret,
                                           const std::string& raw_session_key) {
  if (state_ != STATE_RUNNING || user_id != user_id_)
    return;

  if (type_ != TYPE_SIGNIN) {
    Cancel(user_id_);
    return;
  }

  if (!ScreenlockBridge::Get()->IsLocked())
    return;


  std::string unwrapped_secret = UnwrapSecret(wrapped_secret, raw_session_key);

  // If secret is not set, set it to an arbitrary value, otherwise there will
  // be no authenitcation attempt and the ui will get stuck.
  // TODO(tbarzic): Find a better way to handle this case.
  if (unwrapped_secret.empty())
    unwrapped_secret = kStubSecret;

  std::string key_label;
#if defined(OS_CHROMEOS)
  key_label = chromeos::EasyUnlockKeyManager::GetKeyLabel(0u);
#endif  // defined(OS_CHROMEOS)

  ScreenlockBridge::Get()->lock_handler()->AttemptEasySignin(
      user_id,
      unwrapped_secret,
      key_label);
  state_ = STATE_DONE;
}

void EasyUnlockAuthAttempt::Cancel(const std::string& user_id) {
  if (type_ == TYPE_UNLOCK)
    FinalizeUnlock(user_id, false);
  else
    FinalizeSignin(user_id, "", "");
  state_ = STATE_DONE;
}
