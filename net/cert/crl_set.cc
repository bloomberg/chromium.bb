// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/crl_set.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net {

CRLSet::CRLSet()
    : sequence_(0),
      not_after_(0) {
}

CRLSet::~CRLSet() = default;

CRLSet::Result CRLSet::CheckSPKI(const base::StringPiece& spki_hash) const {
  for (std::vector<std::string>::const_iterator i = blocked_spkis_.begin();
       i != blocked_spkis_.end(); ++i) {
    if (spki_hash.size() == i->size() &&
        memcmp(spki_hash.data(), i->data(), i->size()) == 0) {
      return REVOKED;
    }
  }

  return GOOD;
}

CRLSet::Result CRLSet::CheckSubject(const base::StringPiece& encoded_subject,
                                    const base::StringPiece& spki_hash) const {
  const std::string digest(crypto::SHA256HashString(encoded_subject));
  const auto i = limited_subjects_.find(digest);
  if (i == limited_subjects_.end()) {
    return GOOD;
  }

  for (const auto& j : i->second) {
    if (spki_hash == j) {
      return GOOD;
    }
  }

  return REVOKED;
}

CRLSet::Result CRLSet::CheckSerial(
    const base::StringPiece& serial_number,
    const base::StringPiece& issuer_spki_hash) const {
  base::StringPiece serial(serial_number);

  if (!serial.empty() && (serial[0] & 0x80) != 0) {
    // This serial number is negative but the process which generates CRL sets
    // will reject any certificates with negative serial numbers as invalid.
    return UNKNOWN;
  }

  // Remove any leading zero bytes.
  while (serial.size() > 1 && serial[0] == 0x00)
    serial.remove_prefix(1);

  std::unordered_map<std::string, size_t>::const_iterator crl_index =
      crls_index_by_issuer_.find(issuer_spki_hash.as_string());
  if (crl_index == crls_index_by_issuer_.end())
    return UNKNOWN;
  const std::vector<std::string>& serials = crls_[crl_index->second].second;

  for (std::vector<std::string>::const_iterator i = serials.begin();
       i != serials.end(); ++i) {
    if (base::StringPiece(*i) == serial)
      return REVOKED;
  }

  return GOOD;
}

bool CRLSet::IsExpired() const {
  if (not_after_ == 0)
    return false;

  uint64_t now = base::Time::Now().ToTimeT();
  return now > not_after_;
}

uint32_t CRLSet::sequence() const {
  return sequence_;
}

const CRLSet::CRLList& CRLSet::crls() const {
  return crls_;
}

// static
scoped_refptr<CRLSet> CRLSet::EmptyCRLSetForTesting() {
  return ForTesting(false, NULL, "", "", {});
}

scoped_refptr<CRLSet> CRLSet::ExpiredCRLSetForTesting() {
  return ForTesting(true, NULL, "", "", {});
}

// static
scoped_refptr<CRLSet> CRLSet::ForTesting(
    bool is_expired,
    const SHA256HashValue* issuer_spki,
    const std::string& serial_number,
    const std::string common_name,
    const std::vector<std::string> acceptable_spki_hashes_for_cn) {
  std::string subject_hash;
  if (!common_name.empty()) {
    CBB cbb, top_level, set, inner_seq, oid, cn;
    uint8_t* x501_data;
    size_t x501_len;
    static const uint8_t kCommonNameOID[] = {0x55, 0x04, 0x03};  // 2.5.4.3

    CBB_zero(&cbb);

    if (!CBB_init(&cbb, 32) ||
        !CBB_add_asn1(&cbb, &top_level, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&top_level, &set, CBS_ASN1_SET) ||
        !CBB_add_asn1(&set, &inner_seq, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&inner_seq, &oid, CBS_ASN1_OBJECT) ||
        !CBB_add_bytes(&oid, kCommonNameOID, sizeof(kCommonNameOID)) ||
        !CBB_add_asn1(&inner_seq, &cn, CBS_ASN1_PRINTABLESTRING) ||
        !CBB_add_bytes(&cn,
                       reinterpret_cast<const uint8_t*>(common_name.data()),
                       common_name.size()) ||
        !CBB_finish(&cbb, &x501_data, &x501_len)) {
      CBB_cleanup(&cbb);
      return nullptr;
    }

    subject_hash.assign(crypto::SHA256HashString(
        base::StringPiece(reinterpret_cast<char*>(x501_data), x501_len)));
    OPENSSL_free(x501_data);
  }

  scoped_refptr<CRLSet> crl_set(new CRLSet);
  if (is_expired)
    crl_set->not_after_ = 1;

  if (issuer_spki != NULL) {
    const std::string spki(reinterpret_cast<const char*>(issuer_spki->data),
                           sizeof(issuer_spki->data));
    crl_set->crls_.push_back(make_pair(spki, std::vector<std::string>()));
    crl_set->crls_index_by_issuer_[spki] = 0;
  }

  if (!serial_number.empty())
    crl_set->crls_[0].second.push_back(serial_number);

  if (!subject_hash.empty())
    crl_set->limited_subjects_[subject_hash] = acceptable_spki_hashes_for_cn;

  return crl_set;
}

}  // namespace net
