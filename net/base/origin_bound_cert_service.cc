// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_service.h"

#include <limits>

#include "base/logging.h"
#include "base/rand_util.h"
#include "crypto/rsa_private_key.h"
#include "net/base/x509_certificate.h"

namespace net {

bool OriginBoundCertService::GetOriginBoundCert(const GURL& url,
                                                std::string* private_key_result,
                                                std::string* cert_result) {
  // Check if origin bound cert already exists for this origin.
  if (origin_bound_cert_store_->HasOriginBoundCert(url)) {
    origin_bound_cert_store_->GetOriginBoundCert(url,
                                                 private_key_result,
                                                 cert_result);
    return true;
  }

  // No origin bound cert exists, we have to create one.
  std::string origin = GetCertOriginFromURL(url);
  std::string subject = "CN=origin-bound certificate for " + origin;
  X509Certificate* x509_cert;
  crypto::RSAPrivateKey* key = crypto::RSAPrivateKey::Create(1024);
  if ((x509_cert = X509Certificate::CreateSelfSigned(
      key,
      subject,
      base::RandInt(0, std::numeric_limits<int>::max()),
      base::TimeDelta::FromDays(365))) == NULL) {
    LOG(WARNING) << "Unable to create x509 cert for client";
    return false;
  }

  std::vector<uint8> key_vec;
  if (!key->ExportPrivateKey(&key_vec)) {
    LOG(WARNING) << "Unable to create x509 cert for client";
    return false;
  }
  std::string key_output(key_vec.begin(), key_vec.end());

  std::string cert_output;
  if (!x509_cert->GetDEREncoded(&cert_output)) {
    LOG(WARNING) << "Unable to create x509 cert for client";
    return false;
  }

  origin_bound_cert_store_->SetOriginBoundCert(url, key_output, cert_output);
  *private_key_result = key_output;
  *cert_result = cert_output;

  return true;
}

std::string OriginBoundCertService::GetCertOriginFromURL(const GURL& url) {
  return url.GetOrigin().spec();
}

}  // namespace net
