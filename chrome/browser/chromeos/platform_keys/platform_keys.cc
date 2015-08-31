// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {

namespace platform_keys {

namespace {

void IntersectOnWorkerThread(const net::CertificateList& certs1,
                             const net::CertificateList& certs2,
                             net::CertificateList* intersection) {
  std::map<net::SHA256HashValue, scoped_refptr<net::X509Certificate>,
           net::SHA256HashValueLessThan> fingerprints2;

  // Fill the map with fingerprints of certs from |certs2|.
  for (const auto& cert2 : certs2) {
    fingerprints2[net::X509Certificate::CalculateFingerprint256(
        cert2->os_cert_handle())] = cert2;
  }

  // Compare each cert from |certs1| with the entries of the map.
  for (const auto& cert1 : certs1) {
    const net::SHA256HashValue fingerprint1 =
        net::X509Certificate::CalculateFingerprint256(cert1->os_cert_handle());
    const auto it = fingerprints2.find(fingerprint1);
    if (it == fingerprints2.end())
      continue;
    const auto& cert2 = it->second;
    DCHECK(cert1->Equals(cert2.get()));
    intersection->push_back(cert1);
  }
}

}  // namespace

const char kTokenIdUser[] = "user";
const char kTokenIdSystem[] = "system";

ClientCertificateRequest::ClientCertificateRequest() {
}

ClientCertificateRequest::~ClientCertificateRequest() {
}

void IntersectCertificates(
    const net::CertificateList& certs1,
    const net::CertificateList& certs2,
    const base::Callback<void(scoped_ptr<net::CertificateList>)>& callback) {
  scoped_ptr<net::CertificateList> intersection(new net::CertificateList);
  net::CertificateList* const intersection_ptr = intersection.get();
  if (!base::WorkerPool::PostTaskAndReply(
          FROM_HERE, base::Bind(&IntersectOnWorkerThread, certs1, certs2,
                                intersection_ptr),
          base::Bind(callback, base::Passed(&intersection)),
          false /* task_is_slow */)) {
    callback.Run(make_scoped_ptr(new net::CertificateList));
  }
}

}  // namespace platform_keys

}  // namespace chromeos
