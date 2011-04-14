// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dnssec_keyset.h"

#include <cryptohi.h>
#include <cryptoht.h>
#include <keyhi.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "crypto/nss_util.h"
#include "net/base/dns_util.h"

namespace {

// These are encoded AlgorithmIdentifiers for the given signature algorithm.
const unsigned char kRSAWithSHA1[] = {
  0x30, 0xd, 0x6, 0x9, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0xd, 0x1, 0x1, 0x5, 5, 0
};

const unsigned char kRSAWithSHA256[] = {
  0x30, 0xd, 0x6, 0x9, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0xd, 0x1, 0x1, 0xb, 5, 0
};

}  // namespace

namespace net {

DNSSECKeySet::DNSSECKeySet()
    : ignore_timestamps_(false) {
}

DNSSECKeySet::~DNSSECKeySet() {
}

bool DNSSECKeySet::AddKey(const base::StringPiece& dnskey) {
  uint16 keyid = DNSKEYToKeyID(dnskey);
  std::string der_encoded = ASN1WrapDNSKEY(dnskey);
  if (der_encoded.empty())
    return false;

  keyids_.push_back(keyid);
  public_keys_.push_back(der_encoded);
  return true;
}

bool DNSSECKeySet::CheckSignature(
    const base::StringPiece& name,
    const base::StringPiece& zone,
    const base::StringPiece& signature,
    uint16 rrtype,
    const std::vector<base::StringPiece>& rrdatas) {
  // signature has this format:
  //   algorithm uint8
  //   labels uint8
  //   ttl uint32
  //   expires uint32
  //   begins uint32
  //   keyid uint16
  //
  //   followed by the actual signature.
  if (signature.size() < 16)
    return false;
  const unsigned char* sigdata =
      reinterpret_cast<const unsigned char*>(signature.data());

  uint8 algorithm = sigdata[0];
  uint32 expires = static_cast<uint32>(sigdata[6]) << 24 |
                   static_cast<uint32>(sigdata[7]) << 16 |
                   static_cast<uint32>(sigdata[8]) << 8 |
                   static_cast<uint32>(sigdata[9]);
  uint32 begins = static_cast<uint32>(sigdata[10]) << 24 |
                  static_cast<uint32>(sigdata[11]) << 16 |
                  static_cast<uint32>(sigdata[12]) << 8 |
                  static_cast<uint32>(sigdata[13]);
  uint16 keyid = static_cast<uint16>(sigdata[14]) << 8 |
                 static_cast<uint16>(sigdata[15]);

  if (!ignore_timestamps_) {
    uint32 now = static_cast<uint32>(base::Time::Now().ToTimeT());
    if (now < begins || now >= expires)
      return false;
  }

  base::StringPiece sig(signature.data() + 16, signature.size() - 16);

  // You should have RFC 4034, 3.1.8.1 open when reading this code.
  unsigned signed_data_len = 0;
  signed_data_len += 2;  // rrtype
  signed_data_len += 16;  // (see signature format, above)
  signed_data_len += zone.size();

  for (std::vector<base::StringPiece>::const_iterator
       i = rrdatas.begin(); i != rrdatas.end(); i++) {
    signed_data_len += name.size();
    signed_data_len += 2;  // rrtype
    signed_data_len += 2;  // class
    signed_data_len += 4;  // ttl
    signed_data_len += 2;  // RRDATA length
    signed_data_len += i->size();
  }

  scoped_array<unsigned char> signed_data(new unsigned char[signed_data_len]);
  unsigned j = 0;

  signed_data[j++] = static_cast<uint8>(rrtype >> 8);
  signed_data[j++] = static_cast<uint8>(rrtype);
  memcpy(&signed_data[j], sigdata, 16);
  j += 16;
  memcpy(&signed_data[j], zone.data(), zone.size());
  j += zone.size();

  for (std::vector<base::StringPiece>::const_iterator
       i = rrdatas.begin(); i != rrdatas.end(); i++) {
    memcpy(&signed_data[j], name.data(), name.size());
    j += name.size();
    signed_data[j++] = static_cast<uint8>(rrtype >> 8);
    signed_data[j++] = static_cast<uint8>(rrtype);
    signed_data[j++] = 0;  // CLASS (always IN = 1)
    signed_data[j++] = 1;
    // Copy the TTL from |signature|.
    memcpy(&signed_data[j], signature.data() + 2, sizeof(uint32));
    j += sizeof(uint32);
    unsigned rrdata_len = i->size();
    signed_data[j++] = rrdata_len >> 8;
    signed_data[j++] = rrdata_len;
    memcpy(&signed_data[j], i->data(), i->size());
    j += i->size();
  }

  DCHECK_EQ(j, signed_data_len);

  base::StringPiece signature_algorithm;
  if (algorithm == kDNSSEC_RSA_SHA1 ||
      algorithm == kDNSSEC_RSA_SHA1_NSEC3) {
    signature_algorithm = base::StringPiece(
        reinterpret_cast<const char*>(kRSAWithSHA1),
        sizeof(kRSAWithSHA1));
  } else if (algorithm == kDNSSEC_RSA_SHA256) {
    signature_algorithm = base::StringPiece(
        reinterpret_cast<const char*>(kRSAWithSHA256),
        sizeof(kRSAWithSHA256));
  } else {
    // Unknown algorithm.
    return false;
  }

  // Check the signature with each trusted key which has a matching keyid.
  DCHECK_EQ(public_keys_.size(), keyids_.size());
  for (unsigned i = 0; i < public_keys_.size(); i++) {
    if (keyids_[i] != keyid)
      continue;

    if (VerifySignature(
            signature_algorithm, sig, public_keys_[i],
            base::StringPiece(reinterpret_cast<const char*>(signed_data.get()),
                              signed_data_len))) {
      return true;
    }
  }

  return false;
}

// static
uint16 DNSSECKeySet::DNSKEYToKeyID(const base::StringPiece& dnskey) {
  const unsigned char* data =
      reinterpret_cast<const unsigned char*>(dnskey.data());

  // RFC 4043: App B
  uint32 ac = 0;
  for (unsigned i = 0; i < dnskey.size(); i++) {
    if (i & 1) {
      ac += data[i];
    } else {
      ac += static_cast<uint32>(data[i]) << 8;
    }
  }
  ac += (ac >> 16) & 0xffff;
  return ac;
}

void DNSSECKeySet::IgnoreTimestamps() {
  ignore_timestamps_ = true;
}

bool DNSSECKeySet::VerifySignature(
    base::StringPiece signature_algorithm,
    base::StringPiece signature,
    base::StringPiece public_key,
    base::StringPiece signed_data) {
  // This code is largely a copy-and-paste from
  // crypto/signature_verifier_nss.cc. We can't change
  // crypto::SignatureVerifier to always use NSS because we want the ability to
  // be FIPS 140-2 compliant. However, we can't use crypto::SignatureVerifier
  // here because some platforms don't support SHA256 signatures. Therefore, we
  // use NSS directly.

  crypto::EnsureNSSInit();

  CERTSubjectPublicKeyInfo* spki = NULL;
  SECItem spki_der;
  spki_der.type = siBuffer;
  spki_der.data = (uint8*) public_key.data();
  spki_der.len = public_key.size();
  spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_der);
  if (!spki)
    return false;
  SECKEYPublicKey* pub_key = SECKEY_ExtractPublicKey(spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);  // Done with spki.
  if (!pub_key)
    return false;

  PLArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!arena) {
    SECKEY_DestroyPublicKey(pub_key);
    return false;
  }

  SECItem sig_alg_der;
  sig_alg_der.type = siBuffer;
  sig_alg_der.data = (uint8*) signature_algorithm.data();
  sig_alg_der.len = signature_algorithm.size();
  SECAlgorithmID sig_alg_id;
  SECStatus rv;
  rv = SEC_QuickDERDecodeItem(arena, &sig_alg_id, SECOID_AlgorithmIDTemplate,
                              &sig_alg_der);
  if (rv != SECSuccess) {
    SECKEY_DestroyPublicKey(pub_key);
    PORT_FreeArena(arena, PR_TRUE);
    return false;
  }

  SECItem sig;
  sig.type = siBuffer;
  sig.data = (uint8*) signature.data();
  sig.len = signature.size();
  SECOidTag hash_alg_tag;
  VFYContext* vfy_context =
      VFY_CreateContextWithAlgorithmID(pub_key, &sig,
                                       &sig_alg_id, &hash_alg_tag,
                                       NULL);
  SECKEY_DestroyPublicKey(pub_key);
  PORT_FreeArena(arena, PR_TRUE);  // Done with sig_alg_id.
  if (!vfy_context) {
    // A corrupted RSA signature could be detected without the data, so
    // VFY_CreateContextWithAlgorithmID may fail with SEC_ERROR_BAD_SIGNATURE
    // (-8182).
    return false;
  }

  rv = VFY_Begin(vfy_context);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }
  rv = VFY_Update(vfy_context, (uint8*) signed_data.data(), signed_data.size());
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }
  rv = VFY_End(vfy_context);
  VFY_DestroyContext(vfy_context, PR_TRUE);

  return rv == SECSuccess;
}

// This is an ASN.1 encoded AlgorithmIdentifier for RSA
static const unsigned char kASN1AlgorithmIdentifierRSA[] = {
  0x30,  //   SEQUENCE
  0x0d,  //   length (11 bytes)
  0x06,  //     OBJECT IDENTIFER
  0x09,  //     length (9 bytes)
  //              OID 1.2.840.113549.1.1.1 (RSA)
  0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01,
  //            NULL of length 0
  0x05, 0x00,
};

// EncodeASN1Length assumes that |*length| contains the number of DER-encoded,
// length-prefixed ASN.1 bytes to follow and serialises the length to |out[*j]|
// and updates |j| and |length| accordingly.
static void EncodeASN1Length(unsigned char* out, unsigned* j,
                             unsigned* length) {
  if ((*length - 1) < 128) {
    (*length) -= 1;
    out[(*j)++] = *length;
  } else if ((*length - 2) < 256) {
    (*length) -= 2;
    out[(*j)++] = 0x80 | 1;
    out[(*j)++] = *length;
  } else {
    (*length) -= 3;
    out[(*j)++] = 0x80 | 2;
    out[(*j)++] = *length >> 8;
    out[(*j)++] = *length;
  }
}

// AdvanceForASN1Length returns the number of bytes required to encode a ASN1
// DER length value of |remaining|.
static unsigned AdvanceForASN1Length(unsigned remaining) {
  if (remaining < 128) {
    return 1;
  } else if (remaining < 256) {
    return 2;
  } else if (remaining < 65536) {
    return 3;
  } else {
    NOTREACHED();
    return 3;
  }
}

// ASN1WrapDNSKEY converts the DNSKEY RDATA in |dnskey| into the ASN.1 wrapped
// format expected by NSS. To wit:
//   SubjectPublicKeyInfo  ::=  SEQUENCE  {
//       algorithm            AlgorithmIdentifier,
//       subjectPublicKey     BIT STRING  }
std::string DNSSECKeySet::ASN1WrapDNSKEY(const base::StringPiece& dnskey) {
  const unsigned char* data =
    reinterpret_cast<const unsigned char*>(dnskey.data());

  if (dnskey.size() < 5 || dnskey.size() > 32767)
    return "";
  const uint8 algorithm = data[3];
  if (algorithm != kDNSSEC_RSA_SHA1 &&
      algorithm != kDNSSEC_RSA_SHA1_NSEC3 &&
      algorithm != kDNSSEC_RSA_SHA256) {
    return "";
  }

  unsigned exp_length;
  unsigned exp_offset;
  // First we extract the public exponent.
  if (data[4] == 0) {
    if (dnskey.size() < 7)
      return "";
    exp_length = static_cast<unsigned>(data[5]) << 8 |
                 static_cast<unsigned>(data[6]);
    exp_offset = 7;
  } else {
    exp_length = static_cast<unsigned>(data[4]);
    exp_offset = 5;
  }

  // We refuse to deal with large public exponents.
  if (exp_length > 3)
    return "";
  if (dnskey.size() < exp_offset + exp_length)
    return "";

  unsigned exp = 0;
  for (unsigned i = 0; i < exp_length; i++) {
    exp <<= 8;
    exp |= static_cast<unsigned>(data[exp_offset + i]);
  }

  unsigned n_offset = exp_offset + exp_length;
  unsigned n_length = dnskey.size() - n_offset;

  // Anything smaller than 512 bits is too weak to be trusted.
  if (n_length < 64)
    return "";

  // If the MSB of exp is true then we need to prefix a zero byte to stop the
  // ASN.1 encoding from being negative.
  if (exp & (1 << ((8 * exp_length) - 1)))
    exp_length++;

  // Likewise with the modulus
  unsigned n_padding = data[n_offset] & 0x80 ? 1 : 0;

  // We now calculate the length of the full ASN.1 encoded public key. We're
  // working backwards from the end of the structure. Keep in mind that it's:
  //   SEQUENCE
  //     AlgorithmIdentifier
  //     BITSTRING
  //       SEQUENCE
  //         INTEGER
  //         INTEGER
  unsigned length = 0;
  length += exp_length;  // exponent data
  length++;  // we know that |exp_length| < 128
  length++;  // INTEGER tag for exponent
  length += n_length + n_padding;
  length += AdvanceForASN1Length(n_length + n_padding);
  length++;  // INTEGER tag for modulus
  length += AdvanceForASN1Length(length); // SEQUENCE length
  length++;  // SEQUENCE tag
  length++;  // BITSTRING unused bits
  length += AdvanceForASN1Length(length); // BITSTRING length
  length++;  // BITSTRING tag
  length += sizeof(kASN1AlgorithmIdentifierRSA);
  length += AdvanceForASN1Length(length); // SEQUENCE length
  length++;  // SEQUENCE tag

  scoped_array<unsigned char> out(new unsigned char[length]);

  // Now we walk forwards and serialise the ASN.1, undoing the steps above.
  unsigned j = 0;
  out[j++] = 0x30;  // SEQUENCE
  length--;
  EncodeASN1Length(out.get(), &j, &length);
  memcpy(&out[j], kASN1AlgorithmIdentifierRSA,
         sizeof(kASN1AlgorithmIdentifierRSA));
  j += sizeof(kASN1AlgorithmIdentifierRSA);
  length -= sizeof(kASN1AlgorithmIdentifierRSA);
  out[j++] = 3;  // BITSTRING tag
  length--;
  EncodeASN1Length(out.get(), &j, &length);
  out[j++] = 0;  // BITSTRING unused bits
  length--;
  out[j++] = 0x30;  // SEQUENCE
  length--;
  EncodeASN1Length(out.get(), &j, &length);
  out[j++] = 2;  // INTEGER
  length--;
  unsigned l = n_length + n_padding;
  if (l < 128) {
    out[j++] = l;
    length--;
  } else if (l < 256) {
    out[j++] = 0x80 | 1;
    out[j++] = l;
    length -= 2;
  } else if (l < 65536) {
    out[j++] = 0x80 | 2;
    out[j++] = l >> 8;
    out[j++] = l;
    length -= 3;
  } else {
    NOTREACHED();
  }

  if (n_padding) {
    out[j++] = 0;
    length--;
  }
  memcpy(&out[j], &data[n_offset], n_length);
  j += n_length;
  length -= n_length;
  out[j++] = 2;  // INTEGER
  length--;
  out[j++] = exp_length;
  length--;
  for (unsigned i = exp_length - 1; i < exp_length; i--) {
    out[j++] = exp >> (8 * i);
    length--;
  }

  DCHECK_EQ(0u, length);

  return std::string(reinterpret_cast<char*>(out.get()), j);
}

}  // namespace net
