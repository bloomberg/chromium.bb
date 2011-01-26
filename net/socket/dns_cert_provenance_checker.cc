// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/dns_cert_provenance_checker.h"

#if !defined(USE_OPENSSL)

#include <nspr.h>

#include <hasht.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <sechash.h>

#include <set>
#include <string>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/crypto/encryptor.h"
#include "base/crypto/symmetric_key.h"
#include "base/lazy_instance.h"
#include "base/pickle.h"
#include "base/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/completion_callback.h"
#include "net/base/dns_util.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

namespace {

// A DER encoded SubjectPublicKeyInfo structure containing the server's public
// key.
const uint8 kServerPublicKey[] = {
  0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
  0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
  0x04, 0xc7, 0xea, 0x88, 0x60, 0x52, 0xe3, 0xa3, 0x3e, 0x39, 0x92, 0x0f, 0xa4,
  0x3d, 0xba, 0xd8, 0x02, 0x2d, 0x06, 0x4d, 0x64, 0x98, 0x66, 0xb4, 0x82, 0xf0,
  0x23, 0xa6, 0xd8, 0x37, 0x55, 0x7c, 0x01, 0xbf, 0x18, 0xd8, 0x16, 0x9e, 0x66,
  0xdc, 0x49, 0xbf, 0x2e, 0x86, 0xe3, 0x99, 0xbd, 0xb3, 0x75, 0x25, 0x61, 0x04,
  0x6c, 0x2e, 0xfb, 0x32, 0x42, 0x27, 0xe4, 0x23, 0xea, 0xcd, 0x81, 0x62, 0xc1,
};

const unsigned kMaxUploadsPerSession = 10;

// DnsCertLimits is a singleton class which keeps track of which hosts we have
// uploaded reports for in this session. Since some users will be behind MITM
// proxies, they would otherwise upload for every host and we don't wish to
// spam the upload server.
class DnsCertLimits {
 public:
  DnsCertLimits() { }

  // HaveReachedMaxUploads returns true iff we have uploaded the maximum number
  // of DNS certificate reports for this session.
  bool HaveReachedMaxUploads() {
    return uploaded_hostnames_.size() >= kMaxUploadsPerSession;
  }

  // HaveReachedMaxUploads returns true iff we have already uploaded a report
  // about the given hostname in this session.
  bool HaveUploadedForHostname(const std::string& hostname) {
    return uploaded_hostnames_.count(hostname) > 0;
  }

  void DidUpload(const std::string& hostname) {
    uploaded_hostnames_.insert(hostname);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<DnsCertLimits>;

  std::set<std::string> uploaded_hostnames_;

  DISALLOW_COPY_AND_ASSIGN(DnsCertLimits);
};

static base::LazyInstance<DnsCertLimits> g_dns_cert_limits(
    base::LINKER_INITIALIZED);

// DnsCertProvenanceCheck performs the DNS lookup of the certificate. This
// class is self-deleting.
class DnsCertProvenanceCheck : public base::NonThreadSafe {
 public:
  DnsCertProvenanceCheck(
      const std::string& hostname,
      DnsRRResolver* dnsrr_resolver,
      DnsCertProvenanceChecker::Delegate* delegate,
      const std::vector<base::StringPiece>& der_certs)
      : hostname_(hostname),
        dnsrr_resolver_(dnsrr_resolver),
        delegate_(delegate),
        der_certs_(der_certs.size()),
        handle_(DnsRRResolver::kInvalidHandle),
        ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
              this, &DnsCertProvenanceCheck::ResolutionComplete)) {
    for (size_t i = 0; i < der_certs.size(); i++)
      der_certs_[i] = der_certs[i].as_string();
  }

  void Start() {
    DCHECK(CalledOnValidThread());

    if (der_certs_.empty())
      return;

    DnsCertLimits* const limits = g_dns_cert_limits.Pointer();
    if (limits->HaveReachedMaxUploads() ||
        limits->HaveUploadedForHostname(hostname_)) {
      return;
    }

    uint8 fingerprint[SHA1_LENGTH];
    SECStatus rv = HASH_HashBuf(
        HASH_AlgSHA1, fingerprint, (uint8*) der_certs_[0].data(),
        der_certs_[0].size());
    DCHECK_EQ(SECSuccess, rv);
    char fingerprint_hex[SHA1_LENGTH * 2 + 1];
    for (unsigned i = 0; i < sizeof(fingerprint); i++) {
      static const char hextable[] = "0123456789abcdef";
      fingerprint_hex[i*2] = hextable[fingerprint[i] >> 4];
      fingerprint_hex[i*2 + 1] = hextable[fingerprint[i] & 15];
    }
    fingerprint_hex[SHA1_LENGTH * 2] = 0;

    static const char kBaseCertName[] = ".certs.googlednstest.com";
    domain_.assign(fingerprint_hex);
    domain_.append(kBaseCertName);

    handle_ = dnsrr_resolver_->Resolve(
        domain_, kDNS_TXT, 0 /* flags */, &callback_, &response_,
        0 /* priority */, BoundNetLog());
    if (handle_ == DnsRRResolver::kInvalidHandle) {
      LOG(ERROR) << "Failed to resolve " << domain_ << " for " << hostname_;
      delete this;
    }
  }

 private:
  void ResolutionComplete(int status) {
    DCHECK(CalledOnValidThread());

    if (status == ERR_NAME_NOT_RESOLVED ||
        (status == OK && response_.rrdatas.empty())) {
      LOG(ERROR) << "FAILED"
                 << " hostname:" << hostname_
                 << " domain:" << domain_;
      g_dns_cert_limits.Get().DidUpload(hostname_);
      LogCertificates(der_certs_);
      delegate_->OnDnsCertLookupFailed(hostname_, der_certs_);
    } else if (status == OK) {
      LOG(ERROR) << "GOOD"
                 << " hostname:" << hostname_
                 << " resp:" << response_.rrdatas[0];
    } else {
      LOG(ERROR) << "Unknown error " << status << " for " << domain_;
    }

    delete this;
  }

  // LogCertificates writes a certificate chain, in PEM format, to LOG(ERROR).
  static void LogCertificates(
      const std::vector<std::string>& der_certs) {
    std::string dump;
    bool first = true;

    for (std::vector<std::string>::const_iterator
         i = der_certs.begin(); i != der_certs.end(); i++) {
      if (!first)
        dump += "\n";
      first = false;

      dump += "-----BEGIN CERTIFICATE-----\n";
      std::string b64_encoded;
      base::Base64Encode(*i, &b64_encoded);
      for (size_t i = 0; i < b64_encoded.size();) {
        size_t todo = b64_encoded.size() - i;
        if (todo > 64)
          todo = 64;
        dump += b64_encoded.substr(i, todo);
        dump += "\n";
        i += todo;
      }
      dump += "-----END CERTIFICATE-----";
    }

    LOG(ERROR) << "Offending certificates:\n" << dump;
  }

  const std::string hostname_;
  std::string domain_;
  DnsRRResolver* dnsrr_resolver_;
  DnsCertProvenanceChecker::Delegate* const delegate_;
  std::vector<std::string> der_certs_;
  RRResponse response_;
  DnsRRResolver::Handle handle_;
  CompletionCallbackImpl<DnsCertProvenanceCheck> callback_;
};

SECKEYPublicKey* GetServerPubKey() {
  SECItem der;
  memset(&der, 0, sizeof(der));
  der.data = const_cast<uint8*>(kServerPublicKey);
  der.len = sizeof(kServerPublicKey);

  CERTSubjectPublicKeyInfo* spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&der);
  SECKEYPublicKey* public_key = SECKEY_ExtractPublicKey(spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);

  return public_key;
}

}  // namespace

DnsCertProvenanceChecker::Delegate::~Delegate() {
}

DnsCertProvenanceChecker::~DnsCertProvenanceChecker() {
}

void DnsCertProvenanceChecker::DoAsyncLookup(
    const std::string& hostname,
    const std::vector<base::StringPiece>& der_certs,
    DnsRRResolver* dnsrr_resolver,
    Delegate* delegate) {
  DnsCertProvenanceCheck* check = new DnsCertProvenanceCheck(
      hostname, dnsrr_resolver, delegate, der_certs);
  check->Start();
}

// static
std::string DnsCertProvenanceChecker::BuildEncryptedReport(
    const std::string& hostname,
    const std::vector<std::string>& der_certs) {
  static const int kVersion = 0;
  static const unsigned kKeySizeInBytes = 16;  // AES-128
  static const unsigned kIVSizeInBytes = 16;  // AES's block size
  static const unsigned kPadSize = 4096; // we pad up to 4KB,
  // This is a DER encoded, ANSI X9.62 CurveParams object which simply
  // specifies P256.
  static const uint8 kANSIX962CurveParams[] = {
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
  };

  Pickle p;
  p.WriteString(hostname);
  p.WriteInt(der_certs.size());
  for (std::vector<std::string>::const_iterator
       i = der_certs.begin(); i != der_certs.end(); i++) {
    p.WriteString(*i);
  }
  // We pad to eliminate the possibility that someone could see the size of
  // an upload and use that information to reduce the anonymity set of the
  // certificate chain.
  // The "2*sizeof(uint32)" here covers the padding length which we add next
  // and Pickle's internal length which it includes at the beginning of the
  // data.
  unsigned pad_bytes = kPadSize - ((p.size() + 2*sizeof(uint32)) % kPadSize);
  p.WriteUInt32(pad_bytes);
  char* padding = new char[pad_bytes];
  memset(padding, 0, pad_bytes);
  p.WriteData(padding, pad_bytes);
  delete[] padding;

  // We generate a random public value and perform a DH key agreement with
  // the server's fixed value.
  SECKEYPublicKey* pub_key = NULL;
  SECKEYPrivateKey* priv_key = NULL;
  SECItem ec_der_params;
  memset(&ec_der_params, 0, sizeof(ec_der_params));
  ec_der_params.data = const_cast<uint8*>(kANSIX962CurveParams);
  ec_der_params.len = sizeof(kANSIX962CurveParams);
  priv_key = SECKEY_CreateECPrivateKey(&ec_der_params, &pub_key, NULL);
  SECKEYPublicKey* server_pub_key = GetServerPubKey();

  // This extracts the big-endian, x value of the shared point.
  // The values of the arguments match ssl3_SendECDHClientKeyExchange in NSS
  // 3.12.8's lib/ssl/ssl3ecc.c
  PK11SymKey* pms = PK11_PubDeriveWithKDF(
      priv_key, server_pub_key, PR_FALSE /* is sender */,
      NULL /* random a */, NULL /* random b */, CKM_ECDH1_DERIVE,
      CKM_TLS_MASTER_KEY_DERIVE_DH, CKA_DERIVE, 0 /* key size */,
      CKD_NULL /* KDF */, NULL /* shared data */, NULL /* wincx */);
  SECKEY_DestroyPublicKey(server_pub_key);
  SECStatus rv = PK11_ExtractKeyValue(pms);
  DCHECK_EQ(SECSuccess, rv);
  SECItem* x_data = PK11_GetKeyData(pms);

  // The key and IV are 128-bits and generated from a SHA256 hash of the x
  // value.
  char key_data[SHA256_LENGTH];
  HASH_HashBuf(HASH_AlgSHA256, reinterpret_cast<uint8*>(key_data),
               x_data->data, x_data->len);
  PK11_FreeSymKey(pms);

  DCHECK_GE(sizeof(key_data), kKeySizeInBytes + kIVSizeInBytes);
  std::string raw_key(key_data, kKeySizeInBytes);

  scoped_ptr<base::SymmetricKey> symkey(
      base::SymmetricKey::Import(base::SymmetricKey::AES, raw_key));
  std::string iv(key_data + kKeySizeInBytes, kIVSizeInBytes);

  base::Encryptor encryptor;
  bool r = encryptor.Init(symkey.get(), base::Encryptor::CBC, iv);
  CHECK(r);

  std::string plaintext(reinterpret_cast<const char*>(p.data()), p.size());
  std::string ciphertext;
  encryptor.Encrypt(plaintext, &ciphertext);

  // We use another Pickle object to serialise the 'outer' wrapping of the
  // plaintext.
  Pickle outer;
  outer.WriteInt(kVersion);

  SECItem* pub_key_serialized = SECKEY_EncodeDERSubjectPublicKeyInfo(pub_key);
  outer.WriteString(
      std::string(reinterpret_cast<char*>(pub_key_serialized->data),
                  pub_key_serialized->len));
  SECITEM_FreeItem(pub_key_serialized, PR_TRUE);

  outer.WriteString(ciphertext);

  SECKEY_DestroyPublicKey(pub_key);
  SECKEY_DestroyPrivateKey(priv_key);

  return std::string(reinterpret_cast<const char*>(outer.data()),
                     outer.size());
}

}  // namespace net

#else  // USE_OPENSSL

namespace net {

DnsCertProvenanceChecker::Delegate::~Delegate() {
}

DnsCertProvenanceChecker::~DnsCertProvenanceChecker() {
}

void DnsCertProvenanceChecker::DoAsyncLookup(
    const std::string& hostname,
    const std::vector<base::StringPiece>& der_certs,
    DnsRRResolver* dnsrr_resolver,
    Delegate* delegate) {
}

std::string DnsCertProvenanceChecker::BuildEncryptedReport(
    const std::string& hostname,
    const std::vector<std::string>& der_certs) {
  return "";
}

}  // namespace net

#endif  // USE_OPENSSL
