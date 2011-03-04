// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dnssec_chain_verifier.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/sha1.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "net/base/dns_util.h"
#include "net/base/dnssec_keyset.h"

// We don't have a location for the spec yet, so we'll include it here until it
// finds a better home.

/*
When connecting to a host www.example.com, www.example.com may present a certificate which includes a DNSSEC chain embedded in it. The aim of the embedded chain is to prove that the fingerprint of the public key is valid DNSSEC data. This is achieved by proving a CERT record for the target domain.

Initially, the target domain is constructed by prepending _ssl. For example, the initial target domain for www.example.com is _ssl.www.example.com.

A DNSSEC chain verifier can be in one of two states: entering a zone, or within a zone. Initially, the verifier is entering the root zone.

When entering a zone, the verifier reads the following structure:

uint8 entryKey
uint16 signature length:
  // See RRSIG RDATA in RFC 4043 for details
  uint8 algorithm
  uint8 labels
  uint32 ttl
  uint32 expires
  uint32 begins
  uint16 keyid
  []byte signature
uint8 numKeys
// for each key:
uint16 key length:
  []byte DNSKEY RDATA

|entryKey| indexes the array of DNSKEYs and MUST be less than |numKeys|. The indexed DNSKEY MUST be a key that the verifier trusts, either because it's the long-term root key, or because of a previously presented DS signature.

If only a trusted key is needed within this zone, then the signature length MAY be zero. In which case, |entryKey| MUST be 0 and |numKeys| MUST be 1.

After processing this data, the verifier trusts one or more keys for this zone.

When within a zone, the verifier reads the following structure:

dnsName name
uint16 RRtype

|name| is in DNS format (a series of 8-bit, length prefixed strings). No DNS name compression is permitted.

|name| must be closer to the current target domain than the current zone. Here, 'closer' is defined as a greater number of matching labels when comparing right to left.

|RRtype| may be either DS, CERT or CNAME:

DS: this indicates a zone transition to a new zone named |name|. The verifier reads the following structure:
  uint16 signature length:
    ... (see above for the signature structure)
  uint8 num_ds
  // for each DS:
    uint8 digest_type
    uint16 length
    []byte DS DATA

The verifier is now entering the named zone. It reads ahead and extracts the entry key from the zone entry data and synthisises a DS record for the given digest type and verifies the signature. It then enters the next zone.


CERT: |name| MUST match the target domain. The verifier reads the following structure:
  uint16 signature length:
    ... (see above for the signature structure)
  []byte CERT RDATA

(The format of the CERT RDATA isn't specified here, but the verifier must be able to extract a public key fingerprint in order to validate the original certificate.)

This terminates the verification. There MUST NOT be any more data in the chain.


CNAME: |name| MUST match the target domain. The verifier reads the following structure:
  uint16 signature length:
    ... (see above for the signature structure)
  []byte CNAME RDATA

This replaces the target domain with a new domain. The new domain is the target of the CNAME with _ssl prepended. The verifier is now in the zone that is the greatest common ancestor of the old and new target domains. (For example, when switching from _ssl.www.example.com to _ssl.www.example2.com, the verifier is now in com.)


Example for www.google.com:

The target domain is www.google.com.

The verifier enters ., it already trusts the long-term root key and both root keys are presented in order to extend the trust to the smaller root key.

A DS signature is presented for .com. The verifier is now entering .com.

All four .com keys are presented. The verifier is now in .com.

A DS signature is presented for google.com. The verifier is now entering google.com

As google.com contains only a single DNSKEY, it is included without a signature. The verifier is now in google.com.

A CNAME is presented for www.google.com pointing to www.l.google.com. The target domain is now www.l.google.com. The verifier is now in google.com.

A DS signature is presented for l.google.com. The verifier is now entering l.google.com.

As l.google.com contains only a single DNSKEY, it is included without a signature. The verifier is now in l.google.com.

A CERT record is presented for www.l.google.com. The verification is complete.
*/

namespace {

// This is the 2048-bit DNS root key: http://www.iana.org/dnssec
// 19036 8 2 49AAC11D7B6F6446702E54A1607371607A1A41855200FD2CE1CDDE32F24E8FB5
const unsigned char kRootKey[] = {
 0x01, 0x01, 0x03, 0x08, 0x03, 0x01, 0x00, 0x01, 0xa8, 0x00, 0x20, 0xa9, 0x55,
 0x66, 0xba, 0x42, 0xe8, 0x86, 0xbb, 0x80, 0x4c, 0xda, 0x84, 0xe4, 0x7e, 0xf5,
 0x6d, 0xbd, 0x7a, 0xec, 0x61, 0x26, 0x15, 0x55, 0x2c, 0xec, 0x90, 0x6d, 0x21,
 0x16, 0xd0, 0xef, 0x20, 0x70, 0x28, 0xc5, 0x15, 0x54, 0x14, 0x4d, 0xfe, 0xaf,
 0xe7, 0xc7, 0xcb, 0x8f, 0x00, 0x5d, 0xd1, 0x82, 0x34, 0x13, 0x3a, 0xc0, 0x71,
 0x0a, 0x81, 0x18, 0x2c, 0xe1, 0xfd, 0x14, 0xad, 0x22, 0x83, 0xbc, 0x83, 0x43,
 0x5f, 0x9d, 0xf2, 0xf6, 0x31, 0x32, 0x51, 0x93, 0x1a, 0x17, 0x6d, 0xf0, 0xda,
 0x51, 0xe5, 0x4f, 0x42, 0xe6, 0x04, 0x86, 0x0d, 0xfb, 0x35, 0x95, 0x80, 0x25,
 0x0f, 0x55, 0x9c, 0xc5, 0x43, 0xc4, 0xff, 0xd5, 0x1c, 0xbe, 0x3d, 0xe8, 0xcf,
 0xd0, 0x67, 0x19, 0x23, 0x7f, 0x9f, 0xc4, 0x7e, 0xe7, 0x29, 0xda, 0x06, 0x83,
 0x5f, 0xa4, 0x52, 0xe8, 0x25, 0xe9, 0xa1, 0x8e, 0xbc, 0x2e, 0xcb, 0xcf, 0x56,
 0x34, 0x74, 0x65, 0x2c, 0x33, 0xcf, 0x56, 0xa9, 0x03, 0x3b, 0xcd, 0xf5, 0xd9,
 0x73, 0x12, 0x17, 0x97, 0xec, 0x80, 0x89, 0x04, 0x1b, 0x6e, 0x03, 0xa1, 0xb7,
 0x2d, 0x0a, 0x73, 0x5b, 0x98, 0x4e, 0x03, 0x68, 0x73, 0x09, 0x33, 0x23, 0x24,
 0xf2, 0x7c, 0x2d, 0xba, 0x85, 0xe9, 0xdb, 0x15, 0xe8, 0x3a, 0x01, 0x43, 0x38,
 0x2e, 0x97, 0x4b, 0x06, 0x21, 0xc1, 0x8e, 0x62, 0x5e, 0xce, 0xc9, 0x07, 0x57,
 0x7d, 0x9e, 0x7b, 0xad, 0xe9, 0x52, 0x41, 0xa8, 0x1e, 0xbb, 0xe8, 0xa9, 0x01,
 0xd4, 0xd3, 0x27, 0x6e, 0x40, 0xb1, 0x14, 0xc0, 0xa2, 0xe6, 0xfc, 0x38, 0xd1,
 0x9c, 0x2e, 0x6a, 0xab, 0x02, 0x64, 0x4b, 0x28, 0x13, 0xf5, 0x75, 0xfc, 0x21,
 0x60, 0x1e, 0x0d, 0xee, 0x49, 0xcd, 0x9e, 0xe9, 0x6a, 0x43, 0x10, 0x3e, 0x52,
 0x4d, 0x62, 0x87, 0x3d,
};

// kRootKeyID is the key id for kRootKey
const uint16 kRootKeyID = 19036;

// CountLabels returns the number of DNS labels in |a|, which must be in DNS,
// length-prefixed form.
unsigned CountLabels(base::StringPiece a) {
  for (unsigned c = 0;; c++) {
    if (!a.size())
      return c;
    uint8 label_len = a.data()[0];
    a.remove_prefix(1);
    DCHECK_GE(a.size(), label_len);
    a.remove_prefix(label_len);
  }
}

// RemoveLeadingLabel removes the first label from |a|, which must be in DNS,
// length-prefixed form.
void RemoveLeadingLabel(base::StringPiece* a) {
  if (!a->size())
    return;
  uint8 label_len = a->data()[0];
  a->remove_prefix(1);
  a->remove_prefix(label_len);
}

}  // namespace

namespace net {

struct DNSSECChainVerifier::Zone {
  base::StringPiece name;
  // The number of consecutive labels which |name| shares with |target_|,
  // counting right-to-left from the root.
  unsigned matching_labels;
  DNSSECKeySet trusted_keys;
  Zone* prev;
};

DNSSECChainVerifier::DNSSECChainVerifier(const std::string& target,
                                         const base::StringPiece& chain)
    : current_zone_(NULL),
      target_(target),
      chain_(chain),
      ignore_timestamps_(false),
      valid_(false),
      already_entered_zone_(false),
      rrtype_(0) {
}

DNSSECChainVerifier::~DNSSECChainVerifier() {
  for (std::vector<void*>::iterator
       i = scratch_pool_.begin(); i != scratch_pool_.end(); i++) {
    free(*i);
  }

  Zone* next;
  for (Zone* cur = current_zone_; cur; cur = next) {
    next = cur->prev;
    delete cur;
  }
}

void DNSSECChainVerifier::IgnoreTimestamps() {
  ignore_timestamps_ = true;
}

DNSSECChainVerifier::Error DNSSECChainVerifier::Verify() {
  Error err;

  err = EnterRoot();
  if (err != OK)
    return err;

  for (;;) {
    base::StringPiece next_name;
    err = LeaveZone(&next_name);
    if (err != OK)
      return err;
    if (valid_) {
      if (!chain_.empty())
        return BAD_DATA;  // no trailing data allowed.
      break;
    }

    if (already_entered_zone_) {
      already_entered_zone_ = false;
    } else {
      err = EnterZone(next_name);
      if (err != OK)
        return err;
    }
  }

  return OK;
}

uint16 DNSSECChainVerifier::rrtype() const {
  DCHECK(valid_);
  return rrtype_;
}

const std::vector<base::StringPiece>& DNSSECChainVerifier::rrdatas() const {
  DCHECK(valid_);
  return rrdatas_;
}

// static
std::map<std::string, std::string>
DNSSECChainVerifier::ParseTLSTXTRecord(base::StringPiece rrdata) {
  std::map<std::string, std::string> ret;

  if (rrdata.empty())
    return ret;

  std::string txt;
  txt.reserve(rrdata.size());

  // TXT records are a series of 8-bit length prefixed substrings that we
  // concatenate into |txt|
  while (!rrdata.empty()) {
    unsigned len = rrdata[0];
    if (len == 0 || len + 1 > rrdata.size())
      return ret;
    txt.append(rrdata.data() + 1, len);
    rrdata.remove_prefix(len + 1);
  }

  // We append a space to |txt| to make the parsing code, below, cleaner.
  txt.append(" ");

  // RECORD = KV (' '+ KV)*
  // KV = KEY '=' VALUE
  // KEY = [a-zA-Z0-9]+
  // VALUE = [^ \0]*

  enum State {
    STATE_KEY,
    STATE_VALUE,
    STATE_SPACE,
  };

  State state = STATE_KEY;

  std::map<std::string, std::string> m;

  unsigned start = 0;
  std::string key;

  for (unsigned i = 0; i < txt.size(); i++) {
    char c = txt[i];
    if (c == 0)
      return ret;  // NUL values are never allowed.

    switch (state) {
      case STATE_KEY:
        if (c == '=') {
          if (i == start)
            return ret;  // zero length keys are not allowed.
          key = txt.substr(start, i - start);
          start = i + 1;
          state = STATE_VALUE;
          continue;
        }
        if (!IsAsciiAlpha(c) && !IsAsciiDigit(c))
          return ret;  // invalid key value
        break;
      case STATE_VALUE:
        if (c == ' ') {
          if (m.find(key) == m.end())
            m.insert(make_pair(key, txt.substr(start, i - start)));
          state = STATE_SPACE;
          continue;
        }
        break;
      case STATE_SPACE:
        if (c != ' ') {
          start = i;
          i--;
          state = STATE_KEY;
          continue;
        }
        break;
      default:
        NOTREACHED();
        return ret;
    }
  }

  if (state != STATE_SPACE)
    return ret;

  ret.swap(m);
  return ret;
}

// MatchingLabels returns the number of labels which |a| and |b| share,
// counting right-to-left from the root. |a| and |b| must be DNS,
// length-prefixed names. All names match at the root label, so this always
// returns a value >= 1.

// static
unsigned DNSSECChainVerifier::MatchingLabels(base::StringPiece a,
                                             base::StringPiece b) {
  unsigned c = 0;
  unsigned a_labels = CountLabels(a);
  unsigned b_labels = CountLabels(b);

  while (a_labels > b_labels) {
    RemoveLeadingLabel(&a);
    a_labels--;
  }
  while (b_labels > a_labels) {
    RemoveLeadingLabel(&b);
    b_labels--;
  }

  for (;;) {
    if (!a.size()) {
      if (!b.size())
        return c;
      return 0;
    }
    if (!b.size())
      return 0;
    uint8 a_length = a.data()[0];
    a.remove_prefix(1);
    uint8 b_length = b.data()[0];
    b.remove_prefix(1);
    DCHECK_GE(a.size(), a_length);
    DCHECK_GE(b.size(), b_length);

    if (a_length == b_length && memcmp(a.data(), b.data(), a_length) == 0) {
      c++;
    } else {
      c = 0;
    }

    a.remove_prefix(a_length);
    b.remove_prefix(b_length);
  }
}

// U8 reads, and removes, a single byte from |chain_|
bool DNSSECChainVerifier::U8(uint8* v) {
  if (chain_.size() < 1)
    return false;
  *v = chain_[0];
  chain_.remove_prefix(1);
  return true;
}

// U16 reads, and removes, a big-endian uint16 from |chain_|
bool DNSSECChainVerifier::U16(uint16* v) {
  if (chain_.size() < 2)
    return false;
  const uint8* data = reinterpret_cast<const uint8*>(chain_.data());
  *v = static_cast<uint16>(data[0]) << 8 |
       static_cast<uint16>(data[1]);
  chain_.remove_prefix(2);
  return true;
}

// VariableLength16 reads, and removes, a big-endian, uint16, length-prefixed
// chunk from |chain_|
bool DNSSECChainVerifier::VariableLength16(base::StringPiece* v) {
  uint16 length;
  if (!U16(&length))
    return false;
  if (chain_.size() < length)
    return false;
  *v = chain_.substr(0, length);
  chain_.remove_prefix(length);
  return true;
}

// ReadName reads, and removes, an 8-bit length prefixed DNS name from |chain_|
bool DNSSECChainVerifier::ReadName(base::StringPiece* v) {
  base::StringPiece saved = chain_;
  unsigned length = 0;
  static const uint8 kMaxDNSLabelLen = 63;

  for (;;) {
    if (chain_.size() < 1)
      return false;
    uint8 label_len = chain_.data()[0];
    chain_.remove_prefix(1);
    if (label_len > kMaxDNSLabelLen)
      return false;
    length += 1 + label_len;

    if (label_len == 0)
      break;

    if (chain_.size() < label_len)
      return false;
    chain_.remove_prefix(label_len);
  }

  *v = base::StringPiece(saved.data(), length);
  return true;
}

// ReadAheadEntryKey returns the entry key when |chain_| is positioned at the
// start of a zone.
bool DNSSECChainVerifier::ReadAheadEntryKey(base::StringPiece* v) {
  base::StringPiece saved = chain_;

  uint8 entry_key;
  base::StringPiece sig;
  if (!U8(&entry_key) ||
      !VariableLength16(&sig)) {
    return false;
  }

  if (!ReadAheadKey(v, entry_key))
    return false;
  chain_ = saved;
  return true;
}

// ReadAheadKey returns the entry key when |chain_| is positioned at the start
// of a list of keys.
bool DNSSECChainVerifier::ReadAheadKey(base::StringPiece* v, uint8 entry_key) {
  base::StringPiece saved = chain_;

  uint8 num_keys;
  if (!U8(&num_keys))
    return false;

  for (unsigned i = 0; i < num_keys; i++) {
    if (!VariableLength16(v))
      return false;
    if (i == entry_key) {
      chain_ = saved;
      return true;
    }
  }

  return false;
}

bool DNSSECChainVerifier::ReadDNSKEYs(std::vector<base::StringPiece>* out,
                                      bool is_root) {
  uint8 num_keys;
  if (!U8(&num_keys))
    return false;

  for (unsigned i = 0; i < num_keys; i++) {
    base::StringPiece key;
    if (!VariableLength16(&key))
      return false;
    if (key.empty()) {
      if (!is_root)
        return false;
      key = base::StringPiece(reinterpret_cast<const char*>(kRootKey),
                              sizeof(kRootKey));
    }

    out->push_back(key);
  }

  return true;
}

// DigestKey calculates a DS digest as specified in
// http://tools.ietf.org/html/rfc4034#section-5.1.4
//   name: the DNS form name of the key
//   dnskey: the DNSKEY's RRDATA
//   digest_type: see http://tools.ietf.org/html/rfc4034#appendix-A.2
//   keyid: the key's id
//   algorithm: see http://tools.ietf.org/html/rfc4034#appendix-A.1
bool DNSSECChainVerifier::DigestKey(base::StringPiece* out,
                                    const base::StringPiece& name,
                                    const base::StringPiece& dnskey,
                                    uint8 digest_type,
                                    uint16 keyid,
                                    uint8 algorithm) {
  std::string temp;
  uint8 temp2[base::SHA256_LENGTH];
  const uint8* digest;
  unsigned digest_len;

  std::string input = name.as_string() + dnskey.as_string();

  if (digest_type == kDNSSEC_SHA1) {
    temp = base::SHA1HashString(input);
    digest = reinterpret_cast<const uint8*>(temp.data());
    digest_len = base::SHA1_LENGTH;
  } else if (digest_type == kDNSSEC_SHA256) {
    base::SHA256HashString(input, temp2, sizeof(temp2));
    digest = temp2;
    digest_len = sizeof(temp2);
  } else {
    return false;
  }

  uint8* output = static_cast<uint8*>(malloc(4 + digest_len));
  scratch_pool_.push_back(output);
  output[0] = static_cast<uint8>(keyid >> 8);
  output[1] = static_cast<uint8>(keyid);
  output[2] = algorithm;
  output[3] = digest_type;
  memcpy(output + 4, digest, digest_len);
  *out = base::StringPiece(reinterpret_cast<char*>(output), 4 + digest_len);
  return true;
}

// EnterRoot enters the root zone at the beginning of the chain. This is
// special because no DS record lead us here: we have to validate that the
// entry key is the DNS root key that we already know and trust. Additionally,
// for the root zone only, the keyid of the entry key is prepended to the data.
DNSSECChainVerifier::Error DNSSECChainVerifier::EnterRoot() {
  uint16 root_keyid;

  if (!U16(&root_keyid))
    return BAD_DATA;

  if (root_keyid != kRootKeyID)
    return UNKNOWN_ROOT_KEY;

  base::StringPiece root_key;
  if (!ReadAheadEntryKey(&root_key))
    return BAD_DATA;

  // If the root key is given then it must match the expected root key exactly.
  if (root_key.size()) {
    if (root_key.size() != sizeof(kRootKey) ||
        memcmp(root_key.data(), kRootKey, sizeof(kRootKey))) {
      return UNKNOWN_ROOT_KEY;
    }
  }

  base::StringPiece root("", 1);
  return EnterZone(root);
}

// EnterZone enters a new DNS zone. On entry it's assumed that the entry key
// has been validated.
DNSSECChainVerifier::Error DNSSECChainVerifier::EnterZone(
    const base::StringPiece& zone) {
  Zone* prev = current_zone_;
  current_zone_ = new Zone;
  current_zone_->prev = prev;
  current_zone_->name = zone;
  current_zone_->matching_labels = MatchingLabels(target_, zone);
  if (ignore_timestamps_)
    current_zone_->trusted_keys.IgnoreTimestamps();

  uint8 entry_key;
  base::StringPiece sig;
  if (!U8(&entry_key) ||
      !VariableLength16(&sig)) {
    return BAD_DATA;
  }

  base::StringPiece key;
  if (!ReadAheadKey(&key, entry_key))
    return BAD_DATA;

  if (zone.size() == 1 && key.empty()) {
    // If a key is omitted in the root zone then it's the root key.
    key = base::StringPiece(reinterpret_cast<const char*>(kRootKey),
                            sizeof(kRootKey));
  }
  if (!current_zone_->trusted_keys.AddKey(key))
    return BAD_DATA;

  std::vector<base::StringPiece> dnskeys;
  if (!ReadDNSKEYs(&dnskeys, zone.size() == 1))
    return BAD_DATA;

  if (sig.empty()) {
    // An omitted signature on the keys means that only the entry key is used.
    if (dnskeys.size() > 1 || entry_key != 0)
      return BAD_DATA;
    return OK;
  }

  if (!current_zone_->trusted_keys.CheckSignature(
          zone, zone, sig, kDNS_DNSKEY, dnskeys)) {
    return BAD_SIGNATURE;
  }

  // Add all the keys as trusted.
  for (unsigned i = 0; i < dnskeys.size(); i++) {
    if (i == entry_key)
      continue;
    current_zone_->trusted_keys.AddKey(dnskeys[i]);
  }

  return OK;
}

// LeaveZone transitions out of the current zone, either by following DS
// records to validate the entry key of the next zone, or because the final
// resource records are given.
DNSSECChainVerifier::Error DNSSECChainVerifier::LeaveZone(
    base::StringPiece* next_name) {
  base::StringPiece sig;
  uint16 rrtype;
  Error err;

  if (!ReadName(next_name) ||
      !U16(&rrtype) ||
      !VariableLength16(&sig)) {
    return BAD_DATA;
  }

  std::vector<base::StringPiece> rrdatas;

  if (rrtype == kDNS_DS) {
    err = ReadDSSet(&rrdatas, *next_name);
  } else if (rrtype == kDNS_CERT || rrtype == kDNS_TXT) {
    err = ReadGenericRRs(&rrdatas);
  } else if (rrtype == kDNS_CNAME) {
    err = ReadCNAME(&rrdatas);
  } else {
    return UNKNOWN_TERMINAL_RRTYPE;
  }
  if (err != OK)
    return err;

  if (!current_zone_->trusted_keys.CheckSignature(
      *next_name, current_zone_->name, sig, rrtype, rrdatas)) {
    return BAD_SIGNATURE;
  }

  if (rrtype == kDNS_DS) {
    // If we are transitioning to another zone then the next zone must be
    // 'closer' to the target than the current zone.
    if (MatchingLabels(target_, *next_name) <= current_zone_->matching_labels)
      return OFF_COURSE;
  } else if (rrtype == kDNS_CERT || rrtype == kDNS_TXT) {
    // If this is the final entry in the chain then the name must match target_
    if (next_name->size() != target_.size() ||
        memcmp(next_name->data(), target_.data(), target_.size())) {
      return BAD_TARGET;
    }
    rrdatas_ = rrdatas;
    valid_ = true;
    rrtype_ = rrtype;
  } else if (rrtype == kDNS_CNAME) {
    // A CNAME must match the current target. Then we update the current target
    // and unwind the chain to the closest common ancestor.
    if (next_name->size() != target_.size() ||
        memcmp(next_name->data(), target_.data(), target_.size())) {
      return BAD_TARGET;
    }
    DCHECK_EQ(1u, rrdatas.size());
    target_ = rrdatas[0].as_string();
    // We unwind the zones until the current zone is a (non-strict) subset of
    // the new target.
    while (MatchingLabels(target_, current_zone_->name) <
           CountLabels(current_zone_->name)) {
      Zone* prev = current_zone_->prev;
      delete current_zone_;
      current_zone_ = prev;
      if (!current_zone_) {
        NOTREACHED();
        return BAD_DATA;
      }
    }
    already_entered_zone_ = true;
  } else {
    NOTREACHED();
    return UNKNOWN_TERMINAL_RRTYPE;
  }

  return OK;
}

// ReadDSSet reads a set of DS records from the chain. DS records which are
// omitted are calculated from the entry key of the next zone.
DNSSECChainVerifier::Error DNSSECChainVerifier::ReadDSSet(
    std::vector<base::StringPiece>* rrdatas,
    const base::StringPiece& next_name) {
  uint8 num_ds;
  if (!U8(&num_ds))
    return BAD_DATA;
  scoped_array<uint8> digest_types(new uint8[num_ds]);
  // lookahead[i] is true iff the i'th DS record was empty and needs to be
  // computed by hashing the next entry key.
  scoped_array<bool> lookahead(new bool[num_ds]);
  rrdatas->resize(num_ds);

  for (unsigned i = 0; i < num_ds; i++) {
    uint8 digest_type;
    base::StringPiece digest;
    if (!U8(&digest_type) ||
        !VariableLength16(&digest)) {
      return BAD_DATA;
    }

    digest_types[i] = digest_type;
    lookahead[i] = digest.empty();
    if (!digest.empty())
      (*rrdatas)[i] = digest;
  }

  base::StringPiece next_entry_key;
  if (!ReadAheadEntryKey(&next_entry_key))
    return BAD_DATA;
  if (next_entry_key.size() < 4)
    return BAD_DATA;
  uint16 keyid = DNSSECKeySet::DNSKEYToKeyID(next_entry_key);
  uint8 algorithm = next_entry_key[3];

  bool good = false;
  for (unsigned i = 0; i < num_ds; i++) {
    base::StringPiece digest;
    bool have_digest = false;
    if (DigestKey(&digest, next_name, next_entry_key, digest_types[i],
                  keyid, algorithm)) {
      have_digest = true;
    }

    if (lookahead[i]) {
      // If we needed to fill in one of the DS entries, but we can't calculate
      // that type of digest, then we can't continue.
      if (!have_digest)
        return UNKNOWN_DIGEST;
      (*rrdatas)[i] = digest;
      good = true;
    } else {
      const base::StringPiece& given_digest = (*rrdatas)[i];
      if (have_digest &&
          given_digest.size() == digest.size() &&
          memcmp(given_digest.data(), digest.data(), digest.size()) == 0) {
        good = true;
      }
    }
  }

  if (!good) {
    // We didn't calculate or match any of the digests.
    return NO_DS_LINK;
  }

  return OK;
}

DNSSECChainVerifier::Error DNSSECChainVerifier::ReadGenericRRs(
    std::vector<base::StringPiece>* rrdatas) {
  uint8 num_rrs;
  if (!U8(&num_rrs))
    return BAD_DATA;
  rrdatas->resize(num_rrs);

  for (unsigned i = 0; i < num_rrs; i++) {
    base::StringPiece rrdata;
    if (!VariableLength16(&rrdata))
      return BAD_DATA;
    (*rrdatas)[i] = rrdata;
  }

  return OK;
}

DNSSECChainVerifier::Error DNSSECChainVerifier::ReadCNAME(
    std::vector<base::StringPiece>* rrdatas) {
  base::StringPiece name;
  if (!ReadName(&name))
    return BAD_DATA;

  rrdatas->resize(1);
  (*rrdatas)[0] = name;
  return OK;
}

}  // namespace net
