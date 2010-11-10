// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/dns_cert_provenance_check.h"

#include <nspr.h>

#include <hasht.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <sechash.h>

#include <string>

#include "base/crypto/encryptor.h"
#include "base/crypto/symmetric_key.h"
#include "base/non_thread_safe.h"
#include "base/pickle.h"
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

class DNSCertProvenanceChecker : public NonThreadSafe {
 public:
  DNSCertProvenanceChecker(const std::string hostname,
                           DnsRRResolver* dnsrr_resolver,
                           const std::vector<base::StringPiece>& der_certs)
      : hostname_(hostname),
        dnsrr_resolver_(dnsrr_resolver),
        der_certs_(der_certs.size()),
        handle_(DnsRRResolver::kInvalidHandle),
        ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
              this, &DNSCertProvenanceChecker::ResolutionComplete)) {
    for (size_t i = 0; i < der_certs.size(); i++)
      der_certs_[i] = der_certs[i].as_string();
  }

  void Start() {
    DCHECK(CalledOnValidThread());

    if (der_certs_.empty())
      return;

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

    static const char kBaseCertName[] = ".certs.links.org";
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
      BuildRecord();
    } else if (status == OK) {
      LOG(ERROR) << "GOOD"
                 << " hostname:" << hostname_
                 << " resp:" << response_.rrdatas[0];
    } else {
      LOG(ERROR) << "Unknown error " << status << " for " << domain_;
    }

    delete this;
  }

  // BuildRecord encrypts the certificate chain to a fixed public key and
  // returns the encrypted blob. Since this code is reporting a possible HTTPS
  // failure, it would seem silly to use HTTPS to protect the uploaded report.
  std::string BuildRecord() {
    static const int kVersion = 0;
    static const unsigned kKeySizeInBytes = 16;  // AES-128
    static const unsigned kIVSizeInBytes = 16;  // AES's block size
    static const unsigned kPadSize = 4096; // we pad up to 4KB,
    // This is a DER encoded, ANSI X9.62 CurveParams object which simply
    // specifies P256.
    static const uint8 kANSIX962CurveParams[] = {
      0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07
    };

    DCHECK(CalledOnValidThread());

    Pickle p;
    p.WriteString(hostname_);
    p.WriteInt(der_certs_.size());
    for (std::vector<std::string>::const_iterator
         i = der_certs_.begin(); i != der_certs_.end(); i++) {
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

  SECKEYPublicKey* GetServerPubKey() {
    DCHECK(CalledOnValidThread());

    SECItem der;
    memset(&der, 0, sizeof(der));
    der.data = const_cast<uint8*>(kServerPublicKey);
    der.len = sizeof(kServerPublicKey);

    CERTSubjectPublicKeyInfo* spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&der);
    SECKEYPublicKey* public_key = SECKEY_ExtractPublicKey(spki);
    SECKEY_DestroySubjectPublicKeyInfo(spki);

    return public_key;
  }

  const std::string hostname_;
  std::string domain_;
  DnsRRResolver* const dnsrr_resolver_;
  std::vector<std::string> der_certs_;
  RRResponse response_;
  DnsRRResolver::Handle handle_;
  CompletionCallbackImpl<DNSCertProvenanceChecker> callback_;
};

}  // anonymous namespace

void DoAsyncDNSCertProvenanceVerification(
    const std::string& hostname,
    DnsRRResolver* dnsrr_resolver,
    const std::vector<base::StringPiece>& der_certs) {
  DNSCertProvenanceChecker* c(new DNSCertProvenanceChecker(
      hostname, dnsrr_resolver, der_certs));
  c->Start();
}

}  // namespace net
