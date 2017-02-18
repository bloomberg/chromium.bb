// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cert_loader.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {

static CertLoader* g_cert_loader = NULL;
static bool g_force_hardware_backed_for_test = false;

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
    : certificates_loaded_(false),
      certificates_update_required_(false),
      certificates_update_running_(false),
      database_(NULL),
      cert_list_(new net::CertificateList),
      weak_factory_(this) {
}

CertLoader::~CertLoader() {
  net::CertDatabase::GetInstance()->RemoveObserver(this);
}

void CertLoader::StartWithNSSDB(net::NSSCertDatabase* database) {
  CHECK(!database_);
  database_ = database;

  // Start observing cert database for changes.
  // Observing net::CertDatabase is preferred over observing |database_|
  // directly, as |database_| observers receive only events generated directly
  // by |database_|, so they may miss a few relevant ones.
  // TODO(tbarzic): Once singleton NSSCertDatabase is removed, investigate if
  // it would be OK to observe |database_| directly; or change NSSCertDatabase
  // to send notification on all relevant changes.
  net::CertDatabase::GetInstance()->AddObserver(this);

  LoadCertificates();
}

void CertLoader::AddObserver(CertLoader::Observer* observer) {
  observers_.AddObserver(observer);
}

void CertLoader::RemoveObserver(CertLoader::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
bool CertLoader::IsCertificateHardwareBacked(const net::X509Certificate* cert) {
  if (g_force_hardware_backed_for_test)
    return true;
  PK11SlotInfo* slot = cert->os_cert_handle()->slot;
  return slot && PK11_IsHW(slot);
}

bool CertLoader::CertificatesLoading() const {
  return database_ && !certificates_loaded_;
}

// static
void CertLoader::ForceHardwareBackedForTesting() {
  g_force_hardware_backed_for_test = true;
}

// static
//
// For background see this discussion on dev-tech-crypto.lists.mozilla.org:
// http://web.archiveorange.com/archive/v/6JJW7E40sypfZGtbkzxX
//
// NOTE: This function relies on the convention that the same PKCS#11 ID
// is shared between a certificate and its associated private and public
// keys.  I tried to implement this with PK11_GetLowLevelKeyIDForCert(),
// but that always returns NULL on Chrome OS for me.
std::string CertLoader::GetPkcs11IdAndSlotForCert(
    const net::X509Certificate& cert,
    int* slot_id) {
  DCHECK(slot_id);

  CERTCertificateStr* cert_handle = cert.os_cert_handle();
  SECKEYPrivateKey *priv_key =
      PK11_FindKeyByAnyCert(cert_handle, NULL /* wincx */);
  if (!priv_key)
    return std::string();

  *slot_id = static_cast<int>(PK11_GetSlotID(priv_key->pkcs11Slot));

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

void CertLoader::LoadCertificates() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "LoadCertificates: " << certificates_update_running_;

  if (certificates_update_running_) {
    certificates_update_required_ = true;
    return;
  }

  certificates_update_running_ = true;
  certificates_update_required_ = false;

  database_->ListCerts(
      base::Bind(&CertLoader::UpdateCertificates, weak_factory_.GetWeakPtr()));
}

void CertLoader::UpdateCertificates(
    std::unique_ptr<net::CertificateList> cert_list) {
  CHECK(thread_checker_.CalledOnValidThread());
  DCHECK(certificates_update_running_);
  VLOG(1) << "UpdateCertificates: " << cert_list->size();

  // Ignore any existing certificates.
  cert_list_ = std::move(cert_list);

  bool initial_load = !certificates_loaded_;
  certificates_loaded_ = true;
  NotifyCertificatesLoaded(initial_load);

  certificates_update_running_ = false;
  if (certificates_update_required_)
    LoadCertificates();
}

void CertLoader::NotifyCertificatesLoaded(bool initial_load) {
  for (auto& observer : observers_)
    observer.OnCertificatesLoaded(*cert_list_, initial_load);
}

void CertLoader::OnCertDBChanged() {
  VLOG(1) << "OnCertDBChanged";
  LoadCertificates();
}

}  // namespace chromeos
