// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/wifi_config_model.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"  // g_browser_process
#include "chrome/common/net/x509_certificate_model.h"
#include "net/base/cert_database.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util_collator.h"  // CompareString16WithCollator
#include "unicode/coll.h"  // icu::Collator

namespace chromeos {

namespace {

typedef scoped_refptr<net::X509Certificate> X509CertificateRefPtr;

// Root CA certificates that are built into Chrome use this token name.
const char* const kRootCertificateTokenName = "Builtin Object Token";

// Returns a user-visible name for a given certificate.
string16 GetCertDisplayString(const net::X509Certificate* cert) {
  DCHECK(cert);
  std::string name_or_nick =
      x509_certificate_model::GetCertNameOrNickname(cert->os_cert_handle());
  return UTF8ToUTF16(name_or_nick);
}

// Comparison functor for locale-sensitive sorting of certificates by name.
class CertNameComparator {
 public:
  explicit CertNameComparator(icu::Collator* collator)
      : collator_(collator) {
  }

  bool operator()(const X509CertificateRefPtr& lhs,
                  const X509CertificateRefPtr& rhs) const {
    string16 lhs_name = GetCertDisplayString(lhs);
    string16 rhs_name = GetCertDisplayString(rhs);
    if (collator_ == NULL)
      return lhs_name < rhs_name;
    return l10n_util::CompareString16WithCollator(
        collator_, lhs_name, rhs_name) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

WifiConfigModel::WifiConfigModel() {
}

WifiConfigModel::~WifiConfigModel() {
}

void WifiConfigModel::UpdateCertificates() {
  // CertDatabase and its wrappers do not have random access to certificates,
  // so build filtered lists once.
  net::CertificateList cert_list;
  cert_db_.ListCerts(&cert_list);
  for (net::CertificateList::const_iterator it = cert_list.begin();
       it != cert_list.end();
       ++it) {
    net::X509Certificate* cert = it->get();
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    net::CertType type = x509_certificate_model::GetType(cert_handle);
    switch (type) {
      case net::USER_CERT:
        user_certs_.push_back(*it);
        break;
      case net::CA_CERT: {
        // Exclude root CA certificates that are built into Chrome.
        std::string token_name =
            x509_certificate_model::GetTokenName(cert_handle);
        if (token_name != kRootCertificateTokenName)
          server_ca_certs_.push_back(*it);
        break;
      }
      default:
        // We only care about those two types.
        break;
    }
  }

  // Perform locale-sensitive sorting by certificate name.
  scoped_ptr<icu::Collator> collator;
  UErrorCode error = U_ZERO_ERROR;
  collator.reset(
      icu::Collator::createInstance(
          icu::Locale(g_browser_process->GetApplicationLocale().c_str()),
          error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  CertNameComparator cert_name_comparator(collator.get());
  std::sort(user_certs_.begin(), user_certs_.end(), cert_name_comparator);
  std::sort(server_ca_certs_.begin(), server_ca_certs_.end(),
            cert_name_comparator);
}

int WifiConfigModel::GetUserCertCount() const {
  return static_cast<int>(user_certs_.size());
}

string16 WifiConfigModel::GetUserCertName(int cert_index) const {
  DCHECK(cert_index >= 0);
  DCHECK(cert_index < static_cast<int>(user_certs_.size()));
  net::X509Certificate* cert = user_certs_[cert_index].get();
  return GetCertDisplayString(cert);
}

std::string WifiConfigModel::GetUserCertPkcs11Id(int cert_index) const {
  DCHECK(cert_index >= 0);
  DCHECK(cert_index < static_cast<int>(user_certs_.size()));
  net::X509Certificate* cert = user_certs_[cert_index].get();
  net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
  return x509_certificate_model::GetPkcs11Id(cert_handle);
}

int WifiConfigModel::GetUserCertIndex(const std::string& pkcs11_id) const {
  // The list of user certs is small, so just test each one.
  for (int index = 0; index < static_cast<int>(user_certs_.size()); ++index) {
    net::X509Certificate* cert = user_certs_[index].get();
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    std::string id = x509_certificate_model::GetPkcs11Id(cert_handle);
    if (id == pkcs11_id)
      return index;
  }
  // Not found.
  return -1;
}

int WifiConfigModel::GetServerCaCertCount() const {
  return static_cast<int>(server_ca_certs_.size());
}

string16 WifiConfigModel::GetServerCaCertName(int cert_index) const {
  DCHECK(cert_index >= 0);
  DCHECK(cert_index < static_cast<int>(server_ca_certs_.size()));
  net::X509Certificate* cert = server_ca_certs_[cert_index].get();
  return GetCertDisplayString(cert);
}

std::string WifiConfigModel::GetServerCaCertNssNickname(int cert_index) const {
  DCHECK(cert_index >= 0);
  DCHECK(cert_index < static_cast<int>(server_ca_certs_.size()));
  net::X509Certificate* cert = server_ca_certs_[cert_index].get();
  net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
  return x509_certificate_model::GetNickname(cert_handle);
}

int WifiConfigModel::GetServerCaCertIndex(
    const std::string& nss_nickname) const {
  // List of server certs is small, so just test each one.
  for (int i = 0; i < static_cast<int>(server_ca_certs_.size()); ++i) {
    net::X509Certificate* cert = server_ca_certs_[i].get();
    net::X509Certificate::OSCertHandle cert_handle = cert->os_cert_handle();
    std::string nickname = x509_certificate_model::GetNickname(cert_handle);
    if (nickname == nss_nickname)
      return i;
  }
  // Not found.
  return -1;
}

}  // namespace chromeos
