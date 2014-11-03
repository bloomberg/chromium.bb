// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_chromeos.h"

#include <cert.h>
#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"

namespace net {

namespace {

class CertNotAllowedPredicate {
 public:
  explicit CertNotAllowedPredicate(
      const ClientCertStoreChromeOS::CertFilter& filter)
      : filter_(filter) {}
  bool operator()(const scoped_refptr<X509Certificate>& cert) const {
    return !filter_.IsCertAllowed(cert);
  }

 private:
  const ClientCertStoreChromeOS::CertFilter& filter_;
};

}  // namespace

ClientCertStoreChromeOS::ClientCertStoreChromeOS(
    scoped_ptr<CertFilter> cert_filter,
    const PasswordDelegateFactory& password_delegate_factory)
    : ClientCertStoreNSS(password_delegate_factory),
      cert_filter_(cert_filter.Pass()) {
}

ClientCertStoreChromeOS::~ClientCertStoreChromeOS() {}

void ClientCertStoreChromeOS::GetClientCerts(
    const SSLCertRequestInfo& cert_request_info,
    CertificateList* selected_certs,
    const base::Closure& callback) {
  base::Closure bound_callback =
      base::Bind(&ClientCertStoreChromeOS::CertFilterInitialized,
                 // Caller is responsible for keeping the ClientCertStore alive
                 // until the callback is run.
                 base::Unretained(this),
                 &cert_request_info,
                 selected_certs,
                 callback);

  if (cert_filter_->Init(bound_callback))
    bound_callback.Run();
}

void ClientCertStoreChromeOS::GetClientCertsImpl(
    CERTCertList* cert_list,
    const SSLCertRequestInfo& request,
    bool query_nssdb,
    CertificateList* selected_certs) {
  ClientCertStoreNSS::GetClientCertsImpl(
      cert_list, request, query_nssdb, selected_certs);

  size_t pre_size = selected_certs->size();
  selected_certs->erase(std::remove_if(selected_certs->begin(),
                                       selected_certs->end(),
                                       CertNotAllowedPredicate(*cert_filter_)),
                        selected_certs->end());
  DVLOG(1) << "filtered " << pre_size - selected_certs->size() << " of "
           << pre_size << " certs";
}

void ClientCertStoreChromeOS::CertFilterInitialized(
    const SSLCertRequestInfo* request,
    CertificateList* selected_certs,
    const base::Closure& callback) {
  ClientCertStoreNSS::GetClientCerts(*request, selected_certs, callback);
}

}  // namespace net
