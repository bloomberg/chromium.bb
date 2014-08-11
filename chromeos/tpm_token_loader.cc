// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/tpm_token_loader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "crypto/nss_util.h"

namespace chromeos {

namespace {

const int64 kInitialRequestDelayMs = 100;
const int64 kMaxRequestDelayMs = 300000;  // 5 minutes

// Calculates the delay before running next attempt to initiatialize the TPM
// token, if |last_delay| was the last or initial delay.
base::TimeDelta GetNextRequestDelayMs(base::TimeDelta last_delay) {
  // This implements an exponential backoff, as we don't know in which order of
  // magnitude the TPM token changes it's state.
  base::TimeDelta next_delay = last_delay * 2;

  // Cap the delay to prevent an overflow. This threshold is arbitrarily chosen.
  const base::TimeDelta max_delay =
      base::TimeDelta::FromMilliseconds(kMaxRequestDelayMs);
  if (next_delay > max_delay)
    next_delay = max_delay;
  return next_delay;
}

void PostResultToTaskRunner(scoped_refptr<base::SequencedTaskRunner> runner,
                            const base::Callback<void(bool)>& callback,
                            bool success) {
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

}  // namespace

static TPMTokenLoader* g_tpm_token_loader = NULL;

// static
void TPMTokenLoader::Initialize() {
  CHECK(!g_tpm_token_loader);
  g_tpm_token_loader = new TPMTokenLoader(false /*for_test*/);
}

// static
void TPMTokenLoader::InitializeForTest() {
  CHECK(!g_tpm_token_loader);
  g_tpm_token_loader = new TPMTokenLoader(true /*for_test*/);
}

// static
void TPMTokenLoader::Shutdown() {
  CHECK(g_tpm_token_loader);
  delete g_tpm_token_loader;
  g_tpm_token_loader = NULL;
}

// static
TPMTokenLoader* TPMTokenLoader::Get() {
  CHECK(g_tpm_token_loader)
      << "TPMTokenLoader::Get() called before Initialize()";
  return g_tpm_token_loader;
}

// static
bool TPMTokenLoader::IsInitialized() {
  return g_tpm_token_loader;
}

TPMTokenLoader::TPMTokenLoader(bool for_test)
    : initialized_for_test_(for_test),
      tpm_token_state_(TPM_STATE_UNKNOWN),
      tpm_request_delay_(
          base::TimeDelta::FromMilliseconds(kInitialRequestDelayMs)),
      tpm_token_slot_id_(-1),
      weak_factory_(this) {
  if (!initialized_for_test_ && LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  if (initialized_for_test_) {
    tpm_token_state_ = TPM_TOKEN_INITIALIZED;
    tpm_user_pin_ = "111111";
  }
}

void TPMTokenLoader::SetCryptoTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& crypto_task_runner) {
  crypto_task_runner_ = crypto_task_runner;
  MaybeStartTokenInitialization();
}

TPMTokenLoader::~TPMTokenLoader() {
  if (!initialized_for_test_ && LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

void TPMTokenLoader::AddObserver(TPMTokenLoader::Observer* observer) {
  observers_.AddObserver(observer);
}

void TPMTokenLoader::RemoveObserver(TPMTokenLoader::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TPMTokenLoader::IsTPMTokenReady() const {
  return tpm_token_state_ == TPM_DISABLED ||
         tpm_token_state_ == TPM_TOKEN_INITIALIZED;
}

void TPMTokenLoader::MaybeStartTokenInitialization() {
  CHECK(thread_checker_.CalledOnValidThread());

  // This is the entry point to the TPM token initialization process,
  // which we should do at most once.
  if (tpm_token_state_ != TPM_STATE_UNKNOWN || !crypto_task_runner_.get())
    return;

  if (!LoginState::IsInitialized())
    return;

  bool start_initialization = LoginState::Get()->IsUserLoggedIn();

  VLOG(1) << "StartTokenInitialization: " << start_initialization;
  if (!start_initialization)
    return;

  if (!base::SysInfo::IsRunningOnChromeOS())
    tpm_token_state_ = TPM_DISABLED;

  // Treat TPM as disabled for guest users since they do not store certs.
  if (LoginState::Get()->IsGuestSessionUser())
    tpm_token_state_ = TPM_DISABLED;

  ContinueTokenInitialization();

  DCHECK_NE(tpm_token_state_, TPM_STATE_UNKNOWN);
}

void TPMTokenLoader::ContinueTokenInitialization() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "ContinueTokenInitialization: " << tpm_token_state_;

  switch (tpm_token_state_) {
    case TPM_STATE_UNKNOWN: {
      crypto_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&crypto::EnableTPMTokenForNSS),
          base::Bind(&TPMTokenLoader::OnTPMTokenEnabledForNSS,
                     weak_factory_.GetWeakPtr()));
      tpm_token_state_ = TPM_INITIALIZATION_STARTED;
      return;
    }
    case TPM_INITIALIZATION_STARTED: {
      NOTREACHED();
      return;
    }
    case TPM_TOKEN_ENABLED_FOR_NSS: {
      DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsEnabled(
          base::Bind(&TPMTokenLoader::OnTpmIsEnabled,
                     weak_factory_.GetWeakPtr()));
      return;
    }
    case TPM_DISABLED: {
      // TPM is disabled, so proceed with empty tpm token name.
      NotifyTPMTokenReady();
      return;
    }
    case TPM_ENABLED: {
      DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11IsTpmTokenReady(
          base::Bind(&TPMTokenLoader::OnPkcs11IsTpmTokenReady,
                     weak_factory_.GetWeakPtr()));
      return;
    }
    case TPM_TOKEN_READY: {
      // Retrieve user_pin_ here since they will never change
      // and CryptohomeClient calls are not thread safe.
      DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11GetTpmTokenInfo(
          base::Bind(&TPMTokenLoader::OnPkcs11GetTpmTokenInfo,
                     weak_factory_.GetWeakPtr()));
      return;
    }
    case TPM_TOKEN_INFO_RECEIVED: {
      crypto_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &crypto::InitializeTPMTokenAndSystemSlot,
              tpm_token_slot_id_,
              base::Bind(&PostResultToTaskRunner,
                         base::MessageLoopProxy::current(),
                         base::Bind(&TPMTokenLoader::OnTPMTokenInitialized,
                                    weak_factory_.GetWeakPtr()))));
      return;
    }
    case TPM_TOKEN_INITIALIZED: {
      NotifyTPMTokenReady();
      return;
    }
  }
}

void TPMTokenLoader::RetryTokenInitializationLater() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Retry token initialization later.";
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TPMTokenLoader::ContinueTokenInitialization,
                 weak_factory_.GetWeakPtr()),
      tpm_request_delay_);
  tpm_request_delay_ = GetNextRequestDelayMs(tpm_request_delay_);
}

void TPMTokenLoader::OnTPMTokenEnabledForNSS() {
  VLOG(1) << "TPMTokenEnabledForNSS";
  tpm_token_state_ = TPM_TOKEN_ENABLED_FOR_NSS;
  ContinueTokenInitialization();
}

void TPMTokenLoader::OnTpmIsEnabled(DBusMethodCallStatus call_status,
                                    bool tpm_is_enabled) {
  VLOG(1) << "OnTpmIsEnabled: " << tpm_is_enabled;

  if (call_status == DBUS_METHOD_CALL_SUCCESS && tpm_is_enabled)
    tpm_token_state_ = TPM_ENABLED;
  else
    tpm_token_state_ = TPM_DISABLED;

  ContinueTokenInitialization();
}

void TPMTokenLoader::OnPkcs11IsTpmTokenReady(DBusMethodCallStatus call_status,
                                         bool is_tpm_token_ready) {
  VLOG(1) << "OnPkcs11IsTpmTokenReady: " << is_tpm_token_ready;

  if (call_status == DBUS_METHOD_CALL_FAILURE || !is_tpm_token_ready) {
    RetryTokenInitializationLater();
    return;
  }

  tpm_token_state_ = TPM_TOKEN_READY;
  ContinueTokenInitialization();
}

void TPMTokenLoader::OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                                             const std::string& token_name,
                                             const std::string& user_pin,
                                             int token_slot_id) {
  VLOG(1) << "OnPkcs11GetTpmTokenInfo: " << token_name;

  if (call_status == DBUS_METHOD_CALL_FAILURE) {
    RetryTokenInitializationLater();
    return;
  }

  tpm_token_slot_id_ = token_slot_id;
  tpm_user_pin_ = user_pin;
  tpm_token_state_ = TPM_TOKEN_INFO_RECEIVED;

  ContinueTokenInitialization();
}

void TPMTokenLoader::OnTPMTokenInitialized(bool success) {
  VLOG(1) << "OnTPMTokenInitialized: " << success;
  if (!success) {
    RetryTokenInitializationLater();
    return;
  }
  tpm_token_state_ = TPM_TOKEN_INITIALIZED;
  ContinueTokenInitialization();
}

void TPMTokenLoader::NotifyTPMTokenReady() {
  FOR_EACH_OBSERVER(Observer, observers_, OnTPMTokenReady());
}

void TPMTokenLoader::LoggedInStateChanged() {
  VLOG(1) << "LoggedInStateChanged";
  MaybeStartTokenInitialization();
}

}  // namespace chromeos
