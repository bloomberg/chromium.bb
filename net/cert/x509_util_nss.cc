// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_nss.h"

#include <cert.h>  // Must be included before certdb.h
#include <certdb.h>
#include <cryptohi.h>
#include <nss.h>
#include <pk11pub.h>
#include <prerror.h>
#include <secder.h>
#include <secmod.h>
#include <secport.h>
#include <string.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace x509_util {

namespace {

// Microsoft User Principal Name: 1.3.6.1.4.1.311.20.2.3
const uint8_t kUpnOid[] = {0x2b, 0x6,  0x1,  0x4, 0x1,
                           0x82, 0x37, 0x14, 0x2, 0x3};

std::string DecodeAVAValue(CERTAVA* ava) {
  SECItem* decode_item = CERT_DecodeAVAValue(&ava->value);
  if (!decode_item)
    return std::string();
  std::string value(reinterpret_cast<char*>(decode_item->data),
                    decode_item->len);
  SECITEM_FreeItem(decode_item, PR_TRUE);
  return value;
}

// Generates a unique nickname for |slot|, returning |nickname| if it is
// already unique.
//
// Note: The nickname returned will NOT include the token name, thus the
// token name must be prepended if calling an NSS function that expects
// <token>:<nickname>.
// TODO(gspencer): Internationalize this: it's wrong to hard-code English.
std::string GetUniqueNicknameForSlot(const std::string& nickname,
                                     const SECItem* subject,
                                     PK11SlotInfo* slot) {
  int index = 2;
  std::string new_name = nickname;
  std::string temp_nickname = new_name;
  std::string token_name;

  if (!slot)
    return new_name;

  if (!PK11_IsInternalKeySlot(slot)) {
    token_name.assign(PK11_GetTokenName(slot));
    token_name.append(":");

    temp_nickname = token_name + new_name;
  }

  while (SEC_CertNicknameConflict(temp_nickname.c_str(),
                                  const_cast<SECItem*>(subject),
                                  CERT_GetDefaultCertDB())) {
    base::SStringPrintf(&new_name, "%s #%d", nickname.c_str(), index++);
    temp_nickname = token_name + new_name;
  }

  return new_name;
}

// The default nickname of the certificate, based on the certificate type
// passed in.
std::string GetDefaultNickname(CERTCertificate* nss_cert, CertType type) {
  std::string result;
  if (type == USER_CERT && nss_cert->slot) {
    // Find the private key for this certificate and see if it has a
    // nickname.  If there is a private key, and it has a nickname, then
    // return that nickname.
    SECKEYPrivateKey* private_key =
        PK11_FindPrivateKeyFromCert(nss_cert->slot, nss_cert, NULL /*wincx*/);
    if (private_key) {
      char* private_key_nickname = PK11_GetPrivateKeyNickname(private_key);
      if (private_key_nickname) {
        result = private_key_nickname;
        PORT_Free(private_key_nickname);
        SECKEY_DestroyPrivateKey(private_key);
        return result;
      }
      SECKEY_DestroyPrivateKey(private_key);
    }
  }

  switch (type) {
    case CA_CERT: {
      char* nickname = CERT_MakeCANickname(nss_cert);
      result = nickname;
      PORT_Free(nickname);
      break;
    }
    case USER_CERT: {
      std::string subject_name = GetCERTNameDisplayName(&nss_cert->subject);
      if (subject_name.empty()) {
        const char* email = CERT_GetFirstEmailAddress(nss_cert);
        if (email)
          subject_name = email;
      }
      // TODO(gspencer): Internationalize this. It's wrong to assume English
      // here.
      result =
          base::StringPrintf("%s's %s ID", subject_name.c_str(),
                             GetCERTNameDisplayName(&nss_cert->issuer).c_str());
      break;
    }
    case SERVER_CERT: {
      result = GetCERTNameDisplayName(&nss_cert->subject);
      break;
    }
    case OTHER_CERT:
    default:
      break;
  }
  return result;
}

}  // namespace

bool IsSameCertificate(CERTCertificate* a, CERTCertificate* b) {
  DCHECK(a && b);
  if (a == b)
    return true;
  return a->derCert.len == b->derCert.len &&
         memcmp(a->derCert.data, b->derCert.data, a->derCert.len) == 0;
}

bool IsSameCertificate(CERTCertificate* a, const X509Certificate* b) {
#if BUILDFLAG(USE_BYTE_CERTS)
  return a->derCert.len == CRYPTO_BUFFER_len(b->os_cert_handle()) &&
         memcmp(a->derCert.data, CRYPTO_BUFFER_data(b->os_cert_handle()),
                a->derCert.len) == 0;
#else
  return IsSameCertificate(a, b->os_cert_handle());
#endif
}
bool IsSameCertificate(const X509Certificate* a, CERTCertificate* b) {
  return IsSameCertificate(b, a);
}

ScopedCERTCertificate CreateCERTCertificateFromBytes(const uint8_t* data,
                                                     size_t length) {
  crypto::EnsureNSSInit();

  if (!NSS_IsInitialized())
    return NULL;

  SECItem der_cert;
  der_cert.data = const_cast<uint8_t*>(data);
  der_cert.len = base::checked_cast<unsigned>(length);
  der_cert.type = siDERCertBuffer;

  // Parse into a certificate structure.
  return ScopedCERTCertificate(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, nullptr /* nickname */,
      PR_FALSE /* is_perm */, PR_TRUE /* copyDER */));
}

ScopedCERTCertificate CreateCERTCertificateFromX509Certificate(
    const X509Certificate* cert) {
#if BUILDFLAG(USE_BYTE_CERTS)
  return CreateCERTCertificateFromBytes(
      CRYPTO_BUFFER_data(cert->os_cert_handle()),
      CRYPTO_BUFFER_len(cert->os_cert_handle()));
#else
  return DupCERTCertificate(cert->os_cert_handle());
#endif
}

ScopedCERTCertificateList CreateCERTCertificateListFromX509Certificate(
    const X509Certificate* cert) {
  ScopedCERTCertificateList nss_chain;
  nss_chain.reserve(1 + cert->GetIntermediateCertificates().size());
#if BUILDFLAG(USE_BYTE_CERTS)
  ScopedCERTCertificate nss_cert =
      CreateCERTCertificateFromX509Certificate(cert);
  if (!nss_cert)
    return {};
  nss_chain.push_back(std::move(nss_cert));
  for (net::X509Certificate::OSCertHandle intermediate :
       cert->GetIntermediateCertificates()) {
    ScopedCERTCertificate nss_intermediate = CreateCERTCertificateFromBytes(
        CRYPTO_BUFFER_data(intermediate), CRYPTO_BUFFER_len(intermediate));
    if (!nss_intermediate)
      return {};
    nss_chain.push_back(std::move(nss_intermediate));
  }
#else
  nss_chain.push_back(DupCERTCertificate(cert->os_cert_handle()));
  for (net::X509Certificate::OSCertHandle intermediate :
       cert->GetIntermediateCertificates()) {
    nss_chain.push_back(DupCERTCertificate(intermediate));
  }
#endif
  return nss_chain;
}

ScopedCERTCertificateList CreateCERTCertificateListFromBytes(const char* data,
                                                             size_t length,
                                                             int format) {
  CertificateList certs =
      X509Certificate::CreateCertificateListFromBytes(data, length, format);
  ScopedCERTCertificateList nss_chain;
  nss_chain.reserve(certs.size());
  for (const scoped_refptr<X509Certificate>& cert : certs) {
    ScopedCERTCertificate nss_cert =
        CreateCERTCertificateFromX509Certificate(cert.get());
    if (!nss_cert)
      return {};
    nss_chain.push_back(std::move(nss_cert));
  }
  return nss_chain;
}

ScopedCERTCertificate DupCERTCertificate(CERTCertificate* cert) {
  return ScopedCERTCertificate(CERT_DupCertificate(cert));
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCERTCertificate(
    CERTCertificate* nss_cert,
    const std::vector<CERTCertificate*>& nss_chain) {
#if BUILDFLAG(USE_BYTE_CERTS)
  if (!nss_cert || !nss_cert->derCert.len)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      X509Certificate::CreateOSCertHandleFromBytes(
          reinterpret_cast<const char*>(nss_cert->derCert.data),
          nss_cert->derCert.len));
  if (!cert_handle)
    return nullptr;

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.reserve(nss_chain.size());
  X509Certificate::OSCertHandles intermediates_raw;
  intermediates_raw.reserve(nss_chain.size());
  for (const CERTCertificate* nss_intermediate : nss_chain) {
    if (!nss_intermediate || !nss_intermediate->derCert.len)
      return nullptr;
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle(
        X509Certificate::CreateOSCertHandleFromBytes(
            reinterpret_cast<const char*>(nss_intermediate->derCert.data),
            nss_intermediate->derCert.len));
    if (!intermediate_cert_handle)
      return nullptr;
    intermediates_raw.push_back(intermediate_cert_handle.get());
    intermediates.push_back(std::move(intermediate_cert_handle));
  }
  scoped_refptr<X509Certificate> result(
      X509Certificate::CreateFromHandle(cert_handle.get(), intermediates_raw));
  return result;
#else
  return X509Certificate::CreateFromHandle(nss_cert, nss_chain);
#endif
}

scoped_refptr<X509Certificate> CreateX509CertificateFromCERTCertificate(
    CERTCertificate* cert) {
  return CreateX509CertificateFromCERTCertificate(
      cert, std::vector<CERTCertificate*>());
}

bool GetDEREncoded(CERTCertificate* cert, std::string* der_encoded) {
  if (!cert || !cert->derCert.len)
    return false;
  der_encoded->assign(reinterpret_cast<char*>(cert->derCert.data),
                      cert->derCert.len);
  return true;
}

void GetRFC822SubjectAltNames(CERTCertificate* cert_handle,
                              std::vector<std::string>* names) {
  crypto::ScopedSECItem alt_name(SECITEM_AllocItem(NULL, NULL, 0));
  DCHECK(alt_name.get());

  names->clear();
  SECStatus rv = CERT_FindCertExtension(
      cert_handle, SEC_OID_X509_SUBJECT_ALT_NAME, alt_name.get());
  if (rv != SECSuccess)
    return;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  DCHECK(arena.get());

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena.get(), alt_name.get());

  CERTGeneralName* name = alt_name_list;
  while (name) {
    if (name->type == certRFC822Name) {
      names->push_back(
          std::string(reinterpret_cast<char*>(name->name.other.data),
                      name->name.other.len));
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
}

void GetUPNSubjectAltNames(CERTCertificate* cert_handle,
                           std::vector<std::string>* names) {
  crypto::ScopedSECItem alt_name(SECITEM_AllocItem(NULL, NULL, 0));
  DCHECK(alt_name.get());

  names->clear();
  SECStatus rv = CERT_FindCertExtension(
      cert_handle, SEC_OID_X509_SUBJECT_ALT_NAME, alt_name.get());
  if (rv != SECSuccess)
    return;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  DCHECK(arena.get());

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena.get(), alt_name.get());

  CERTGeneralName* name = alt_name_list;
  while (name) {
    if (name->type == certOtherName) {
      OtherName* on = &name->name.OthName;
      if (on->oid.len == sizeof(kUpnOid) &&
          memcmp(on->oid.data, kUpnOid, sizeof(kUpnOid)) == 0) {
        SECItem decoded;
        if (SEC_QuickDERDecodeItem(arena.get(), &decoded,
                                   SEC_ASN1_GET(SEC_UTF8StringTemplate),
                                   &name->name.OthName.name) == SECSuccess) {
          names->push_back(
              std::string(reinterpret_cast<char*>(decoded.data), decoded.len));
        }
      }
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
}

std::string GetDefaultUniqueNickname(CERTCertificate* nss_cert,
                                     CertType type,
                                     PK11SlotInfo* slot) {
  return GetUniqueNicknameForSlot(GetDefaultNickname(nss_cert, type),
                                  &nss_cert->derSubject, slot);
}

std::string GetCERTNameDisplayName(CERTName* name) {
  // Search for attributes in the Name, in this order: CN, O and OU.
  CERTAVA* ou_ava = nullptr;
  CERTAVA* o_ava = nullptr;
  CERTRDN** rdns = name->rdns;
  for (size_t rdn = 0; rdns[rdn]; ++rdn) {
    CERTAVA** avas = rdns[rdn]->avas;
    for (size_t pair = 0; avas[pair] != 0; ++pair) {
      SECOidTag tag = CERT_GetAVATag(avas[pair]);
      if (tag == SEC_OID_AVA_COMMON_NAME) {
        // If CN is found, return immediately.
        return DecodeAVAValue(avas[pair]);
      }
      // If O or OU is found, save the first one of each so that it can be
      // returned later if no CN attribute is found.
      if (tag == SEC_OID_AVA_ORGANIZATION_NAME && !o_ava)
        o_ava = avas[pair];
      if (tag == SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME && !ou_ava)
        ou_ava = avas[pair];
    }
  }
  if (o_ava)
    return DecodeAVAValue(o_ava);
  if (ou_ava)
    return DecodeAVAValue(ou_ava);
  return std::string();
}

}  // namespace x509_util

}  // namespace net
