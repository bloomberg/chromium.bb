// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cert_loader.h"

#include <algorithm>

#include "base/chromeos/chromeos_version.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "crypto/encryptor.h"
#include "crypto/nss_util.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"
#include "net/cert/nss_cert_database.h"

namespace chromeos {

namespace {

const int64 kInitialRequestDelayMs = 100;
const int64 kMaxRequestDelayMs = 300000; // 5 minutes

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

void LoadNSSCertificates(net::CertificateList* cert_list) {
  if (base::chromeos::IsRunningOnChromeOS())
    net::NSSCertDatabase::GetInstance()->ListCerts(cert_list);
}

}  // namespace

CertLoader::CertLoader()
    : certificates_requested_(false),
      certificates_loaded_(false),
      certificates_update_required_(false),
      certificates_update_running_(false),
      tpm_token_state_(TPM_STATE_UNKNOWN),
      tpm_request_delay_(
          base::TimeDelta::FromMilliseconds(kInitialRequestDelayMs)),
      initialize_token_factory_(this),
      update_certificates_factory_(this) {
  net::CertDatabase::GetInstance()->AddObserver(this);
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);
  RequestCertificates();
}

CertLoader::~CertLoader() {
  net::CertDatabase::GetInstance()->RemoveObserver(this);
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

void CertLoader::AddObserver(CertLoader::Observer* observer) {
  observers_.AddObserver(observer);
}

void CertLoader::RemoveObserver(CertLoader::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CertLoader::CertificatesLoading() const {
  return certificates_requested_ && !certificates_loaded_;
}

bool CertLoader::IsHardwareBacked() const {
  return !tpm_token_name_.empty();
}

void CertLoader::RequestCertificates() {
  CHECK(thread_checker_.CalledOnValidThread());
  const bool logged_in = LoginState::IsInitialized() ?
      LoginState::Get()->IsUserLoggedIn() : false;
  VLOG(1) << "RequestCertificates: " << logged_in;
  if (certificates_requested_ || !logged_in)
    return;

  certificates_requested_ = true;

  // Ensure we've opened the user's key/certificate database.
  crypto::OpenPersistentNSSDB();
  if (base::chromeos::IsRunningOnChromeOS())
    crypto::EnableTPMTokenForNSS();

  // This is the entry point to the TPM token initialization process, which we
  // should do at most once.
  DCHECK(!initialize_token_factory_.HasWeakPtrs());
  InitializeTokenAndLoadCertificates();
}

void CertLoader::InitializeTokenAndLoadCertificates() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "InitializeTokenAndLoadCertificates";

  switch (tpm_token_state_) {
    case TPM_STATE_UNKNOWN: {
      DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsEnabled(
      base::Bind(&CertLoader::OnTpmIsEnabled,
                 initialize_token_factory_.GetWeakPtr()));
      return;
    }
    case TPM_DISABLED: {
      // TPM is disabled, so proceed with empty tpm token name.
      StartLoadCertificates();
      return;
    }
    case TPM_ENABLED: {
      DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11IsTpmTokenReady(
          base::Bind(&CertLoader::OnPkcs11IsTpmTokenReady,
                     initialize_token_factory_.GetWeakPtr()));
      return;
    }
    case TPM_TOKEN_READY: {
      // Retrieve token_name_ and user_pin_ here since they will never change
      // and CryptohomeClient calls are not thread safe.
      DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11GetTpmTokenInfo(
          base::Bind(&CertLoader::OnPkcs11GetTpmTokenInfo,
                     initialize_token_factory_.GetWeakPtr()));
      return;
    }
    case TPM_TOKEN_INFO_RECEIVED: {
      InitializeNSSForTPMToken();
      return;
    }
    case TPM_TOKEN_NSS_INITIALIZED: {
      StartLoadCertificates();
      return;
    }
  }
}

void CertLoader::RetryTokenInitializationLater() {
  LOG(WARNING) << "Re-Requesting Certificates later.";
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CertLoader::InitializeTokenAndLoadCertificates,
                 initialize_token_factory_.GetWeakPtr()),
      tpm_request_delay_);
  tpm_request_delay_ = GetNextRequestDelayMs(tpm_request_delay_);
}

// For background see this discussion on dev-tech-crypto.lists.mozilla.org:
// http://web.archiveorange.com/archive/v/6JJW7E40sypfZGtbkzxX
//
// NOTE: This function relies on the convention that the same PKCS#11 ID
// is shared between a certificate and its associated private and public
// keys.  I tried to implement this with PK11_GetLowLevelKeyIDForCert(),
// but that always returns NULL on Chrome OS for me.
std::string CertLoader::GetPkcs11IdForCert(
    const net::X509Certificate& cert) const {
  if (!IsHardwareBacked())
    return std::string();

  CERTCertificateStr* cert_handle = cert.os_cert_handle();
  SECKEYPrivateKey *priv_key =
      PK11_FindKeyByAnyCert(cert_handle, NULL /* wincx */);
  if (!priv_key)
    return std::string();

  // Get the CKA_ID attribute for a key.
  SECItem* sec_item = PK11_GetLowLevelKeyIDForPrivateKey(priv_key);
  std::string pkcs11_id;
  if (sec_item) {
    pkcs11_id = base::HexEncode(sec_item->data, sec_item->len);
    SECITEM_FreeItem(sec_item, PR_TRUE);
  }
  SECKEY_DestroyPrivateKey(priv_key);

  return pkcs11_id;
}

void CertLoader::OnTpmIsEnabled(DBusMethodCallStatus call_status,
                                bool tpm_is_enabled) {
  VLOG(1) << "OnTpmIsEnabled: " << tpm_is_enabled;

  if (call_status == DBUS_METHOD_CALL_SUCCESS && tpm_is_enabled)
    tpm_token_state_ = TPM_ENABLED;
  else
    tpm_token_state_ = TPM_DISABLED;

  InitializeTokenAndLoadCertificates();
}

void CertLoader::OnPkcs11IsTpmTokenReady(DBusMethodCallStatus call_status,
                                         bool is_tpm_token_ready) {
  VLOG(1) << "OnPkcs11IsTpmTokenReady: " << is_tpm_token_ready;

  if (call_status == DBUS_METHOD_CALL_FAILURE || !is_tpm_token_ready) {
    RetryTokenInitializationLater();
    return;
  }

  tpm_token_state_ = TPM_TOKEN_READY;
  InitializeTokenAndLoadCertificates();
}

void CertLoader::OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                                         const std::string& token_name,
                                         const std::string& user_pin) {
  VLOG(1) << "OnPkcs11GetTpmTokenInfo: " << token_name;

  if (call_status == DBUS_METHOD_CALL_FAILURE) {
    RetryTokenInitializationLater();
    return;
  }

  tpm_token_name_ = token_name;
  // TODO(stevenjb): The network code expects a slot ID, not a label. See
  // crbug.com/201101. For now, use a hard coded, well known slot instead.
  const char kHardcodedTpmSlot[] = "0";
  tpm_token_slot_ = kHardcodedTpmSlot;
  tpm_user_pin_ = user_pin;
  tpm_token_state_ = TPM_TOKEN_INFO_RECEIVED;

  InitializeTokenAndLoadCertificates();
}

void CertLoader::InitializeNSSForTPMToken() {
  VLOG(1) << "InitializeNSSForTPMToken";

  if (base::chromeos::IsRunningOnChromeOS() &&
      !crypto::InitializeTPMToken(tpm_token_name_, tpm_user_pin_)) {
    RetryTokenInitializationLater();
    return;
  }

  tpm_token_state_ = TPM_TOKEN_NSS_INITIALIZED;
  InitializeTokenAndLoadCertificates();
}

void CertLoader::StartLoadCertificates() {
  VLOG(1) << "StartLoadCertificates";

  if (certificates_update_running_) {
    certificates_update_required_ = true;
    return;
  }

  net::CertificateList* cert_list = new net::CertificateList;
  certificates_update_running_ = true;
  certificates_update_required_ = false;
  base::WorkerPool::GetTaskRunner(true /* task_is_slow */)->
      PostTaskAndReply(
          FROM_HERE,
          base::Bind(LoadNSSCertificates, cert_list),
          base::Bind(&CertLoader::UpdateCertificates,
                     update_certificates_factory_.GetWeakPtr(),
                     base::Owned(cert_list)));
}

void CertLoader::UpdateCertificates(net::CertificateList* cert_list) {
  CHECK(thread_checker_.CalledOnValidThread());
  DCHECK(certificates_update_running_);
  VLOG(1) << "UpdateCertificates: " << cert_list->size();

  // Ignore any existing certificates.
  cert_list_.swap(*cert_list);

  NotifyCertificatesLoaded(!certificates_loaded_);
  certificates_loaded_ = true;

  certificates_update_running_ = false;
  if (certificates_update_required_)
    StartLoadCertificates();
}

void CertLoader::NotifyCertificatesLoaded(bool initial_load) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCertificatesLoaded(cert_list_, initial_load));
}

void CertLoader::OnCertTrustChanged(const net::X509Certificate* cert) {
}

void CertLoader::OnCertAdded(const net::X509Certificate* cert) {
  VLOG(1) << "OnCertAdded";
  StartLoadCertificates();
}

void CertLoader::OnCertRemoved(const net::X509Certificate* cert) {
  VLOG(1) << "OnCertRemoved";
  StartLoadCertificates();
}

void CertLoader::LoggedInStateChanged(LoginState::LoggedInState state) {
  VLOG(1) << "LoggedInStateChanged: " << state;
  RequestCertificates();
}

}  // namespace chromeos
