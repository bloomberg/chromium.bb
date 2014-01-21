// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cert_loader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "crypto/nss_util.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {

namespace {

// Loads certificates from |cert_database| into |cert_list|.
void LoadNSSCertificates(net::NSSCertDatabase* cert_database,
                         net::CertificateList* cert_list) {
  cert_database->ListCerts(cert_list);
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
    : certificates_requested_(false),
      certificates_loaded_(false),
      certificates_update_required_(false),
      certificates_update_running_(false),
      tpm_token_slot_id_(-1),
      weak_factory_(this) {
  if (TPMTokenLoader::IsInitialized())
    TPMTokenLoader::Get()->AddObserver(this);
}

void CertLoader::SetSlowTaskRunnerForTest(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  slow_task_runner_for_test_ = task_runner;
}

CertLoader::~CertLoader() {
  net::CertDatabase::GetInstance()->RemoveObserver(this);
  if (TPMTokenLoader::IsInitialized())
    TPMTokenLoader::Get()->RemoveObserver(this);
}

void CertLoader::AddObserver(CertLoader::Observer* observer) {
  observers_.AddObserver(observer);
}

void CertLoader::RemoveObserver(CertLoader::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CertLoader::IsHardwareBacked() const {
  return !tpm_token_name_.empty();
}

bool CertLoader::CertificatesLoading() const {
  return certificates_requested_ && !certificates_loaded_;
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

void CertLoader::RequestCertificates() {
  if (certificates_requested_)
    return;
  certificates_requested_ = true;

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
      base::Bind(LoadNSSCertificates,
                 net::NSSCertDatabase::GetInstance(),
                 cert_list),
      base::Bind(&CertLoader::UpdateCertificates,
                 weak_factory_.GetWeakPtr(),
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

void CertLoader::OnTPMTokenReady(const std::string& tpm_user_pin,
                                 const std::string& tpm_token_name,
                                 int tpm_token_slot_id) {
  tpm_user_pin_ = tpm_user_pin;
  tpm_token_name_ = tpm_token_name;
  tpm_token_slot_id_ = tpm_token_slot_id;

  VLOG(1) << "TPM token ready.";
  RequestCertificates();
}

}  // namespace chromeos
