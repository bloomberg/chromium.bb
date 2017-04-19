// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/system_trust_store.h"

#if defined(USE_NSS_CERTS)
#include <cert.h>
#include <pk11pub.h>
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <Security/Security.h>
#endif

#include "base/memory/ptr_util.h"
#include "net/cert/internal/trust_store_collection.h"
#include "net/cert/internal/trust_store_in_memory.h"

#if defined(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#include "net/cert/internal/cert_issuer_source_nss.h"
#include "net/cert/internal/trust_store_nss.h"
#include "net/cert/scoped_nss_types.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "net/cert/internal/trust_store_mac.h"
#endif

namespace net {

namespace {

// Abstract implementation of SystemTrustStore to be used as a base class.
// Handles the addition of additional trust anchors.
class BaseSystemTrustStore : public SystemTrustStore {
 public:
  BaseSystemTrustStore() {
    trust_store_.AddTrustStore(&additional_trust_store_);
  }

  void AddTrustAnchor(const scoped_refptr<TrustAnchor>& trust_anchor) override {
    additional_trust_store_.AddTrustAnchor(trust_anchor);
  }

  TrustStore* GetTrustStore() override { return &trust_store_; }

  CertIssuerSource* GetCertIssuerSource() override { return nullptr; }

  bool IsAdditionalTrustAnchor(
      const scoped_refptr<TrustAnchor>& trust_anchor) const override {
    return additional_trust_store_.Contains(trust_anchor.get());
  }

 protected:
  TrustStoreCollection trust_store_;
  TrustStoreInMemory additional_trust_store_;
};

}  // namespace

#if defined(USE_NSS_CERTS)
namespace {

class SystemTrustStoreNSS : public BaseSystemTrustStore {
 public:
  explicit SystemTrustStoreNSS() : trust_store_nss_(trustSSL) {
    trust_store_.AddTrustStore(&trust_store_nss_);
  }

  CertIssuerSource* GetCertIssuerSource() override {
    return &cert_issuer_source_nss_;
  }

  bool UsesSystemTrustStore() const override { return true; }

  // IsKnownRoot returns true if the given trust anchor is a standard one (as
  // opposed to a user-installed root)
  bool IsKnownRoot(
      const scoped_refptr<TrustAnchor>& trust_anchor) const override {
    // TODO(eroman): Based on how the TrustAnchors are created by this
    // integration, there will always be an associated certificate. However this
    // contradicts the API for TrustAnchor that states it is optional.
    DCHECK(trust_anchor->cert());

    // TODO(eroman): The overall approach of IsKnownRoot() is inefficient -- it
    // requires searching for the trust anchor by DER in NSS, however path
    // building already had a handle to it.
    SECItem der_cert;
    der_cert.data =
        const_cast<uint8_t*>(trust_anchor->cert()->der_cert().UnsafeData());
    der_cert.len = trust_anchor->cert()->der_cert().Length();
    der_cert.type = siDERCertBuffer;
    ScopedCERTCertificate nss_cert(
        CERT_FindCertByDERCert(CERT_GetDefaultCertDB(), &der_cert));
    if (!nss_cert)
      return false;

    return IsKnownRoot(nss_cert.get());
  }

 private:
  // TODO(eroman): This function was copied verbatim from
  // cert_verify_proc_nss.cc
  //
  // IsKnownRoot returns true if the given certificate is one that we believe
  // is a standard (as opposed to user-installed) root.
  bool IsKnownRoot(CERTCertificate* root) const {
    if (!root || !root->slot)
      return false;

    // This magic name is taken from
    // http://bonsai.mozilla.org/cvsblame.cgi?file=mozilla/security/nss/lib/ckfw/builtins/constants.c&rev=1.13&mark=86,89#79
    return 0 == strcmp(PK11_GetSlotName(root->slot), "NSS Builtin Objects");
  }

  TrustStoreNSS trust_store_nss_;
  CertIssuerSourceNSS cert_issuer_source_nss_;
};

}  // namespace

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return base::MakeUnique<SystemTrustStoreNSS>();
}

#elif defined(OS_MACOSX) && !defined(OS_IOS)

class SystemTrustStoreMac : public BaseSystemTrustStore {
 public:
  explicit SystemTrustStoreMac() : trust_store_mac_(kSecPolicyAppleSSL) {
    trust_store_.AddTrustStore(&trust_store_mac_);
  }

  CertIssuerSource* GetCertIssuerSource() override {
    // TODO(eroman): Should this return something?
    return nullptr;
  }

  bool UsesSystemTrustStore() const override { return true; }

  // IsKnownRoot returns true if the given trust anchor is a standard one (as
  // opposed to a user-installed root)
  bool IsKnownRoot(
      const scoped_refptr<TrustAnchor>& trust_anchor) const override {
    // TODO(eroman): Implement.
    return false;
  }

 private:
  TrustStoreMac trust_store_mac_;
};

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return base::MakeUnique<SystemTrustStoreMac>();
}
#else

class DummySystemTrustStore : public BaseSystemTrustStore {
 public:
  bool UsesSystemTrustStore() const override { return false; }

  bool IsKnownRoot(
      const scoped_refptr<TrustAnchor>& trust_anchor) const override {
    return false;
  }
};

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStore() {
  return base::MakeUnique<DummySystemTrustStore>();
}
#endif

}  // namespace net
