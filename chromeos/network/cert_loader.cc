// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/cert_loader.h"

#include <algorithm>

#include "base/chromeos/chromeos_version.h"
#include "base/observer_list.h"
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

// Delay between certificate requests while waiting for TPM/PKCS#11 init.
const int kRequestDelayMs = 500;

net::CertificateList* LoadNSSCertificates() {
  net::CertificateList* cert_list(new net::CertificateList());
  net::NSSCertDatabase::GetInstance()->ListCerts(cert_list);
  return cert_list;
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
    : tpm_token_ready_(false),
      certificates_requested_(false),
      certificates_loaded_(false),
      key_store_loaded_(false),
      weak_ptr_factory_(this) {
  net::CertDatabase::GetInstance()->AddObserver(this);
  LoginState::Get()->AddObserver(this);
  if (LoginState::Get()->IsUserLoggedIn())
    RequestCertificates();
}

CertLoader::~CertLoader() {
  request_task_.Reset();
  net::CertDatabase::GetInstance()->RemoveObserver(this);
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
  VLOG(1) << "RequestCertificates: " << LoginState::Get()->IsUserLoggedIn();
  if (!LoginState::Get()->IsUserLoggedIn())
    return;  // Certificates will be requested on login.

  if (!key_store_loaded_) {
    // Ensure we've opened the real user's key/certificate database.
    crypto::OpenPersistentNSSDB();

    if (base::chromeos::IsRunningOnChromeOS())
      crypto::EnableTPMTokenForNSS();

    key_store_loaded_ = true;
  }

  certificates_requested_ = true;

  VLOG(1) << "Requesting Certificates.";
  DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsEnabled(
      base::Bind(&CertLoader::OnTpmIsEnabled,
                 weak_ptr_factory_.GetWeakPtr()));

  return;
}

void CertLoader::OnTpmIsEnabled(DBusMethodCallStatus call_status,
                                bool tpm_is_enabled) {
  VLOG(1) << "OnTpmIsEnabled: " << tpm_is_enabled;
  if (call_status != DBUS_METHOD_CALL_SUCCESS || !tpm_is_enabled) {
    // TPM is not enabled, so proceed with empty tpm token name.
    VLOG(1) << "TPM not available.";
    StartLoadCertificates();
  } else if (tpm_token_ready_) {
    // Once the TPM token is ready, initialize it.
    InitializeTPMToken();
  } else {
    DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11IsTpmTokenReady(
        base::Bind(&CertLoader::OnPkcs11IsTpmTokenReady,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void CertLoader::OnPkcs11IsTpmTokenReady(DBusMethodCallStatus call_status,
                                         bool is_tpm_token_ready) {
  VLOG(1) << "OnPkcs11IsTpmTokenReady: " << is_tpm_token_ready;
  if (call_status != DBUS_METHOD_CALL_SUCCESS || !is_tpm_token_ready) {
    MaybeRetryRequestCertificates();
    return;
  }

  // Retrieve token_name_ and user_pin_ here since they will never change
  // and CryptohomeClient calls are not thread safe.
  DBusThreadManager::Get()->GetCryptohomeClient()->Pkcs11GetTpmTokenInfo(
      base::Bind(&CertLoader::OnPkcs11GetTpmTokenInfo,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CertLoader::OnPkcs11GetTpmTokenInfo(DBusMethodCallStatus call_status,
                                         const std::string& token_name,
                                         const std::string& user_pin) {
  VLOG(1) << "OnPkcs11GetTpmTokenInfo: " << token_name;
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    MaybeRetryRequestCertificates();
    return;
  }
  tpm_token_name_ = token_name;
  // TODO(stevenjb): The network code expects a slot ID, not a label. See
  // crbug.com/201101. For now, use a hard coded, well known slot instead.
  const char kHardcodedTpmSlot[] = "0";
  tpm_token_slot_ = kHardcodedTpmSlot;
  tpm_user_pin_ = user_pin;
  tpm_token_ready_ = true;

  InitializeTPMToken();
}

void CertLoader::InitializeTPMToken() {
  VLOG(1) << "InitializeTPMToken";
  if (!crypto::InitializeTPMToken(tpm_token_name_, tpm_user_pin_)) {
    MaybeRetryRequestCertificates();
    return;
  }
  StartLoadCertificates();
}

void CertLoader::StartLoadCertificates() {
  VLOG(1) << "Start Load Certificates";
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(true /* task_is_slow */),
      FROM_HERE,
      base::Bind(LoadNSSCertificates),
      base::Bind(&CertLoader::UpdateCertificates,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CertLoader::UpdateCertificates(net::CertificateList* cert_list) {
  CHECK(thread_checker_.CalledOnValidThread());
  DCHECK(cert_list);
  VLOG(1) << "Update Certificates: " << cert_list->size();

  // Clear any existing certificates.
  cert_list_.swap(*cert_list);

  // cert_list is constructed in LoadCertificates.
  delete cert_list;

  // Set loaded state and notify observers.
  if (!certificates_loaded_) {
    certificates_loaded_ = true;
    NotifyCertificatesLoaded(true);
  } else {
    NotifyCertificatesLoaded(false);
  }
}

void CertLoader::MaybeRetryRequestCertificates() {
  if (!request_task_.is_null())
    return;

  // Cryptohome does not notify us when the token is ready, so call
  // this again after a delay.
  request_task_ = base::Bind(&CertLoader::RequestCertificatesTask,
                             weak_ptr_factory_.GetWeakPtr());
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      request_task_,
      base::TimeDelta::FromMilliseconds(kRequestDelayMs));
}

void CertLoader::RequestCertificatesTask() {
  // Reset the task to the initial state so is_null() returns true.
  request_task_.Reset();
  RequestCertificates();
}

void CertLoader::NotifyCertificatesLoaded(bool initial_load) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnCertificatesLoaded(cert_list_, initial_load));
}

void CertLoader::OnCertTrustChanged(const net::X509Certificate* cert) {
}

void CertLoader::OnCertAdded(const net::X509Certificate* cert) {
  // Only load certificates if we have completed an initial request.
  if (!certificates_loaded_)
    return;
  StartLoadCertificates();
}

void CertLoader::OnCertRemoved(const net::X509Certificate* cert) {
  // Only load certificates if we have completed an initial request.
  if (!certificates_loaded_)
    return;
  StartLoadCertificates();
}

void CertLoader::LoggedInStateChanged(LoginState::LoggedInState state) {
  VLOG(1) << "LoggedInStateChanged: " << state;
  if (LoginState::Get()->IsUserLoggedIn() && !certificates_requested_)
    RequestCertificates();
}

}  // namespace chromeos
