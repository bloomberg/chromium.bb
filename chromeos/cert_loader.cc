// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cert_loader.h"

#include <algorithm>

#include "base/message_loop/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
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
  net::NSSCertDatabase::GetInstance()->ListCerts(cert_list);
}

void CallOpenPersistentNSSDB() {
  // Called from crypto_task_runner_.
  VLOG(1) << "CallOpenPersistentNSSDB";

  // Ensure we've opened the user's key/certificate database.
  if (base::SysInfo::IsRunningOnChromeOS())
    crypto::OpenPersistentNSSDB();
  crypto::EnableTPMTokenForNSS();
}

}  // namespace

static CertLoader* g_cert_loader = NULL;

// static
void CertLoader::Initialize() {
  CHECK(!g_cert_loader);
  g_cert_loader = new CertLoader();
}

// static
void CertLoader::Shutdown() {
  CHECK(g_cert_loader);
  delete g_cert_loader;
  g_cert_loader = NULL;
}

// static
CertLoader* CertLoader::Get() {
  CHECK(g_cert_loader) << "CertLoader::Get() called before Initialize()";
  return g_cert_loader;
}

// static
bool CertLoader::IsInitialized() {
  return g_cert_loader;
}

CertLoader::CertLoader()
    : initialize_tpm_for_test_(false),
      certificates_requested_(false),
      certificates_loaded_(false),
      certificates_update_required_(false),
      certificates_update_running_(false),
      tpm_token_state_(TPM_STATE_UNKNOWN),
      tpm_request_delay_(
          base::TimeDelta::FromMilliseconds(kInitialRequestDelayMs)),
      tpm_token_slot_id_(-1),
      initialize_token_factory_(this),
      update_certificates_factory_(this) {
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);
}

void CertLoader::InitializeTPMForTest() {
  initialize_tpm_for_test_ = true;
}

void CertLoader::SetCryptoTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& crypto_task_runner) {
  crypto_task_runner_ = crypto_task_runner;
  MaybeRequestCertificates();
}

void CertLoader::SetSlowTaskRunnerForTest(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  slow_task_runner_for_test_ = task_runner;
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

void CertLoader::MaybeRequestCertificates() {
  CHECK(thread_checker_.CalledOnValidThread());

  // This is the entry point to the TPM token initialization process,
  // which we should do at most once.
  if (certificates_requested_ || !crypto_task_runner_.get())
    return;

  if (!LoginState::IsInitialized())
    return;

  bool request_certificates = LoginState::Get()->IsUserLoggedIn() ||
      LoginState::Get()->IsInSafeMode();

  VLOG(1) << "RequestCertificates: " << request_certificates;
  if (!request_certificates)
    return;

  certificates_requested_ = true;

  // Ensure we only initialize the TPM token once.
  DCHECK_EQ(tpm_token_state_, TPM_STATE_UNKNOWN);
  if (!initialize_tpm_for_test_ && !base::SysInfo::IsRunningOnChromeOS())
    tpm_token_state_ = TPM_DISABLED;

  // Treat TPM as disabled for guest users since they do not store certs.
  if (LoginState::Get()->IsGuestUser())
    tpm_token_state_ = TPM_DISABLED;

  InitializeTokenAndLoadCertificates();
}

void CertLoader::InitializeTokenAndLoadCertificates() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "InitializeTokenAndLoadCertificates: " << tpm_token_state_;

  switch (tpm_token_state_) {
    case TPM_STATE_UNKNOWN: {
      crypto_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&CallOpenPersistentNSSDB),
          base::Bind(&CertLoader::OnPersistentNSSDBOpened,
                     initialize_token_factory_.GetWeakPtr()));
      return;
    }
    case TPM_DB_OPENED: {
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
      base::PostTaskAndReplyWithResult(
          crypto_task_runner_.get(),
          FROM_HERE,
          base::Bind(&crypto::InitializeTPMToken, tpm_token_slot_id_),
          base::Bind(&CertLoader::OnTPMTokenInitialized,
                     initialize_token_factory_.GetWeakPtr()));
      return;
    }
    case TPM_TOKEN_INITIALIZED: {
      StartLoadCertificates();
      return;
    }
  }
}

void CertLoader::RetryTokenInitializationLater() {
  CHECK(thread_checker_.CalledOnValidThread());
  LOG(WARNING) << "Retry token initialization later.";
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CertLoader::InitializeTokenAndLoadCertificates,
                 initialize_token_factory_.GetWeakPtr()),
      tpm_request_delay_);
  tpm_request_delay_ = GetNextRequestDelayMs(tpm_request_delay_);
}

void CertLoader::OnPersistentNSSDBOpened() {
  VLOG(1) << "PersistentNSSDBOpened";
  tpm_token_state_ = TPM_DB_OPENED;
  InitializeTokenAndLoadCertificates();
}

// This is copied from chrome/common/net/x509_certificate_model_nss.cc.
// For background see this discussion on dev-tech-crypto.lists.mozilla.org:
// http://web.archiveorange.com/archive/v/6JJW7E40sypfZGtbkzxX
//
// NOTE: This function relies on the convention that the same PKCS#11 ID
// is shared between a certificate and its associated private and public
// keys.  I tried to implement this with PK11_GetLowLevelKeyIDForCert(),
// but that always returns NULL on Chrome OS for me.

// static
std::string CertLoader::GetPkcs11IdForCert(const net::X509Certificate& cert) {
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
                                         const std::string& user_pin,
                                         int token_slot_id) {
  VLOG(1) << "OnPkcs11GetTpmTokenInfo: " << token_name;

  if (call_status == DBUS_METHOD_CALL_FAILURE) {
    RetryTokenInitializationLater();
    return;
  }

  tpm_token_name_ = token_name;
  tpm_token_slot_id_ = token_slot_id;
  tpm_user_pin_ = user_pin;
  tpm_token_state_ = TPM_TOKEN_INFO_RECEIVED;

  InitializeTokenAndLoadCertificates();
}

void CertLoader::OnTPMTokenInitialized(bool success) {
  VLOG(1) << "OnTPMTokenInitialized: " << success;
  if (!success) {
    RetryTokenInitializationLater();
    return;
  }
  tpm_token_state_ = TPM_TOKEN_INITIALIZED;
  InitializeTokenAndLoadCertificates();
}

void CertLoader::StartLoadCertificates() {
  DCHECK(!certificates_loaded_ && !certificates_update_running_);
  net::CertDatabase::GetInstance()->AddObserver(this);
  LoadCertificates();
}

void CertLoader::LoadCertificates() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "LoadCertificates: " << certificates_update_running_;

  if (certificates_update_running_) {
    certificates_update_required_ = true;
    return;
  }

  net::CertificateList* cert_list = new net::CertificateList;
  certificates_update_running_ = true;
  certificates_update_required_ = false;

  base::TaskRunner* task_runner = slow_task_runner_for_test_.get();
  if (!task_runner)
    task_runner = base::WorkerPool::GetTaskRunner(true /* task is slow */);
  task_runner->PostTaskAndReply(
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

  bool initial_load = !certificates_loaded_;
  certificates_loaded_ = true;
  NotifyCertificatesLoaded(initial_load);

  certificates_update_running_ = false;
  if (certificates_update_required_)
    LoadCertificates();
}

void CertLoader::NotifyCertificatesLoaded(bool initial_load) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCertificatesLoaded(cert_list_, initial_load));
}

void CertLoader::OnCACertChanged(const net::X509Certificate* cert) {
  // This is triggered when a CA certificate is modified.
  VLOG(1) << "OnCACertChanged";
  LoadCertificates();
}

void CertLoader::OnCertAdded(const net::X509Certificate* cert) {
  // This is triggered when a client certificate is added.
  VLOG(1) << "OnCertAdded";
  LoadCertificates();
}

void CertLoader::OnCertRemoved(const net::X509Certificate* cert) {
  VLOG(1) << "OnCertRemoved";
  LoadCertificates();
}

void CertLoader::LoggedInStateChanged() {
  VLOG(1) << "LoggedInStateChanged";
  MaybeRequestCertificates();
}

}  // namespace chromeos
