// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"

#include <cert.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "net/ssl/ssl_cert_request_info.h"

namespace chromeos {

namespace {

class CertNotAllowedPredicate {
 public:
  explicit CertNotAllowedPredicate(
      const ClientCertStoreChromeOS::CertFilter* filter)
      : filter_(filter) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return !filter_->IsCertAllowed(cert);
  }

 private:
  const ClientCertStoreChromeOS::CertFilter* const filter_;
};

}  // namespace

ClientCertStoreChromeOS::ClientCertStoreChromeOS(
    std::unique_ptr<CertificateProvider> cert_provider,
    std::unique_ptr<CertFilter> cert_filter,
    const PasswordDelegateFactory& password_delegate_factory)
    : cert_provider_(std::move(cert_provider)),
      cert_filter_(std::move(cert_filter)) {}

ClientCertStoreChromeOS::~ClientCertStoreChromeOS() {}

void ClientCertStoreChromeOS::GetClientCerts(
    const net::SSLCertRequestInfo& cert_request_info,
    const ClientCertListCallback& callback) {
  // Caller is responsible for keeping the ClientCertStore alive until the
  // callback is run.
  base::Callback<void(const net::CertificateList&)>
      get_platform_certs_and_filter =
          base::Bind(&ClientCertStoreChromeOS::GotAdditionalCerts,
                     base::Unretained(this), &cert_request_info, callback);

  base::Closure get_additional_certs_and_continue;
  if (cert_provider_) {
    get_additional_certs_and_continue = base::Bind(
        &CertificateProvider::GetCertificates,
        base::Unretained(cert_provider_.get()), get_platform_certs_and_filter);
  } else {
    get_additional_certs_and_continue =
        base::Bind(get_platform_certs_and_filter, net::CertificateList());
  }

  if (cert_filter_->Init(get_additional_certs_and_continue))
    get_additional_certs_and_continue.Run();
}

void ClientCertStoreChromeOS::GotAdditionalCerts(
    const net::SSLCertRequestInfo* request,
    const ClientCertListCallback& callback,
    const net::CertificateList& additional_certs) {
  std::unique_ptr<crypto::CryptoModuleBlockingPasswordDelegate>
      password_delegate;
  if (!password_delegate_factory_.is_null()) {
    password_delegate.reset(
        password_delegate_factory_.Run(request->host_and_port));
  }
  if (base::PostTaskAndReplyWithResult(
          base::WorkerPool::GetTaskRunner(true /* task_is_slow */).get(),
          FROM_HERE,
          base::Bind(&ClientCertStoreChromeOS::GetAndFilterCertsOnWorkerThread,
                     base::Unretained(this), base::Passed(&password_delegate),
                     request, additional_certs),
          callback)) {
    return;
  }
  // If the task could not be posted, behave as if there were no certificates.
  callback.Run(net::CertificateList());
}

net::CertificateList ClientCertStoreChromeOS::GetAndFilterCertsOnWorkerThread(
    std::unique_ptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate,
    const net::SSLCertRequestInfo* request,
    const net::CertificateList& additional_certs) {
  net::CertificateList unfiltered_certs;
  net::ClientCertStoreNSS::GetPlatformCertsOnWorkerThread(
      std::move(password_delegate), &unfiltered_certs);

  unfiltered_certs.erase(
      std::remove_if(unfiltered_certs.begin(), unfiltered_certs.end(),
                     CertNotAllowedPredicate(cert_filter_.get())),
      unfiltered_certs.end());

  unfiltered_certs.insert(unfiltered_certs.end(), additional_certs.begin(),
                          additional_certs.end());

  net::CertificateList selected_certs;
  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(unfiltered_certs, *request,
                                                     &selected_certs);
  return selected_certs;
}

}  // namespace chromeos
