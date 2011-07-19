// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_service.h"

#include <limits>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "crypto/rsa_private_key.h"
#include "googleurl/src/gurl.h"
#include "net/base/origin_bound_cert_store.h"
#include "net/base/x509_certificate.h"

namespace net {

namespace {

const int kKeySizeInBits = 1024;
const int kValidityPeriodInDays = 365;

}  // namespace

bool OriginBoundCertService::GetOriginBoundCert(const GURL& url,
                                                std::string* private_key_result,
                                                std::string* cert_result) {
  // Check if origin bound cert already exists for this origin.
  if (origin_bound_cert_store_->HasOriginBoundCert(url)) {
    return origin_bound_cert_store_->GetOriginBoundCert(url,
                                                        private_key_result,
                                                        cert_result);
  }

  // No origin bound cert exists, we have to create one.
  std::string origin = url.GetOrigin().spec();
  std::string subject = "CN=origin-bound certificate for " + origin;
  scoped_ptr<crypto::RSAPrivateKey> key(
      crypto::RSAPrivateKey::Create(kKeySizeInBits));
  if (!key.get()) {
    LOG(WARNING) << "Unable to create key pair for client";
    return false;
  }
  scoped_refptr<X509Certificate> x509_cert = X509Certificate::CreateSelfSigned(
      key.get(),
      subject,
      base::RandInt(0, std::numeric_limits<int>::max()),
      base::TimeDelta::FromDays(kValidityPeriodInDays));
  if (!x509_cert) {
    LOG(WARNING) << "Unable to create x509 cert for client";
    return false;
  }

  std::vector<uint8> private_key_info;
  if (!key->ExportPrivateKey(&private_key_info)) {
    LOG(WARNING) << "Unable to export private key";
    return false;
  }
  // TODO(rkn): Perhaps ExportPrivateKey should be changed to output a
  // std::string* to prevent this copying.
  std::string key_out(private_key_info.begin(), private_key_info.end());

  std::string der_cert;
  if (!x509_cert->GetDEREncoded(&der_cert)) {
    LOG(WARNING) << "Unable to get DER-enconded cert";
    return false;
  }

  if (!origin_bound_cert_store_->SetOriginBoundCert(url, key_out, der_cert)) {
    LOG(WARNING) << "Unable to set origin bound certificate";
    return false;
  }

  private_key_result->swap(key_out);
  cert_result->swap(der_cert);
  return true;
}

}  // namespace net
