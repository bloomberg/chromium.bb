// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util.h"
#include "net/cert/x509_util_openssl.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_cert_types.h"

namespace net {

namespace x509_util {

bool IsSupportedValidityRange(base::Time not_valid_before,
                              base::Time not_valid_after) {
  if (not_valid_before > not_valid_after)
    return false;

  // The validity field of a certificate can only encode years 1-9999.

  // Compute the base::Time values corresponding to Jan 1st,0001 and
  // Jan 1st, 10000 respectively. Done by using the pre-computed numbers
  // of days between these dates and the Unix epoch, i.e. Jan 1st, 1970,
  // using the following Python script:
  //
  //     from datetime import date as D
  //     print (D(1970,1,1)-D(1,1,1))        # -> 719162 days
  //     print (D(9999,12,31)-D(1970,1,1))   # -> 2932896 days
  //
  // Note: This ignores leap seconds, but should be enough in practice.
  //
  const int64 kDaysFromYear0001ToUnixEpoch = 719162;
  const int64 kDaysFromUnixEpochToYear10000 = 2932896 + 1;
  const base::Time kEpoch = base::Time::UnixEpoch();
  const base::Time kYear0001 = kEpoch -
      base::TimeDelta::FromDays(kDaysFromYear0001ToUnixEpoch);
  const base::Time kYear10000 = kEpoch +
      base::TimeDelta::FromDays(kDaysFromUnixEpochToYear10000);

  if (not_valid_before < kYear0001 || not_valid_before >= kYear10000 ||
      not_valid_after < kYear0001 || not_valid_after >= kYear10000)
    return false;

  return true;
}

bool CreateDomainBoundCertEC(
    crypto::ECPrivateKey* key,
    const std::string& domain,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    std::string* der_cert) {
  NOTIMPLEMENTED();
  return false;
}

bool CreateSelfSignedCert(crypto::RSAPrivateKey* key,
                          const std::string& common_name,
                          uint32 serial_number,
                          base::Time not_valid_before,
                          base::Time not_valid_after,
                          std::string* der_encoded) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  static const char kCommonNamePrefix[] = "CN=";
  const size_t kCommonNamePrefixLen = sizeof(kCommonNamePrefix) - 1;

  // Put the serial number into an OpenSSL-friendly object.
  crypto::ScopedOpenSSL<ASN1_INTEGER, ASN1_INTEGER_free> asn1_serial(
      ASN1_INTEGER_new());
  if (!asn1_serial.get() ||
      !ASN1_INTEGER_set(asn1_serial.get(), static_cast<long>(serial_number))) {
    LOG(ERROR) << "Invalid serial number " << serial_number;
    return false;
  }

  // Do the same for the time stamps.
  crypto::ScopedOpenSSL<ASN1_TIME, ASN1_TIME_free> asn1_not_before_time(
      ASN1_TIME_set(NULL, not_valid_before.ToTimeT()));
  if (!asn1_not_before_time.get()) {
    LOG(ERROR) << "Invalid not_valid_before time: "
               << not_valid_before.ToTimeT();
    return false;
  }

  crypto::ScopedOpenSSL<ASN1_TIME, ASN1_TIME_free> asn1_not_after_time(
      ASN1_TIME_set(NULL, not_valid_after.ToTimeT()));
  if (!asn1_not_after_time.get()) {
    LOG(ERROR) << "Invalid not_valid_after time: " << not_valid_after.ToTimeT();
    return false;
  }

  // Because |common_name| only contains a common name and starts with 'CN=',
  // there is no need for a full RFC 2253 parser here. Do some sanity checks
  // though.
  if (common_name.size() < kCommonNamePrefixLen ||
      strncmp(common_name.c_str(), kCommonNamePrefix, kCommonNamePrefixLen)) {
    LOG(ERROR) << "Common name must begin with " << kCommonNamePrefix;
    return false;
  }
  if (common_name.size() > INT_MAX) {
    LOG(ERROR) << "Common name too long";
    return false;
  }
  unsigned char* common_name_str =
      reinterpret_cast<unsigned char*>(const_cast<char*>(common_name.data())) +
      kCommonNamePrefixLen;
  int common_name_len =
      static_cast<int>(common_name.size()) - kCommonNamePrefixLen;

  crypto::ScopedOpenSSL<X509_NAME, X509_NAME_free> name(X509_NAME_new());
  if (!name.get() || !X509_NAME_add_entry_by_NID(name.get(),
                                                 NID_commonName,
                                                 MBSTRING_ASC,
                                                 common_name_str,
                                                 common_name_len,
                                                 -1,
                                                 0)) {
    LOG(ERROR) << "Can't parse common name: " << common_name.c_str();
    return false;
  }

  // Now create certificate and populate it.
  crypto::ScopedOpenSSL<X509, X509_free> cert(X509_new());
  if (!cert.get() || !X509_set_version(cert.get(), 2L) /* i.e. version 3 */ ||
      !X509_set_pubkey(cert.get(), key->key()) ||
      !X509_set_serialNumber(cert.get(), asn1_serial.get()) ||
      !X509_set_notBefore(cert.get(), asn1_not_before_time.get()) ||
      !X509_set_notAfter(cert.get(), asn1_not_after_time.get()) ||
      !X509_set_subject_name(cert.get(), name.get()) ||
      !X509_set_issuer_name(cert.get(), name.get())) {
    LOG(ERROR) << "Could not create certificate";
    return false;
  }

  // Sign it with the private key.
  if (!X509_sign(cert.get(), key->key(), EVP_sha1())) {
    LOG(ERROR) << "Could not sign certificate with key.";
    return false;
  }

  // Convert it into a DER-encoded string copied to |der_encoded|.
  int der_data_length = i2d_X509(cert.get(), NULL);
  if (der_data_length < 0)
    return false;

  der_encoded->resize(static_cast<size_t>(der_data_length));
  unsigned char* der_data =
      reinterpret_cast<unsigned char*>(&(*der_encoded)[0]);

  if (i2d_X509(cert.get(), &der_data) < 0)
    return false;

  return true;
}

bool ParsePrincipalKeyAndValueByIndex(X509_NAME* name,
                                      int index,
                                      std::string* key,
                                      std::string* value) {
  X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, index);
  if (!entry)
    return false;

  if (key) {
    ASN1_OBJECT* object = X509_NAME_ENTRY_get_object(entry);
    key->assign(OBJ_nid2sn(OBJ_obj2nid(object)));
  }

  ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
  if (!data)
    return false;

  unsigned char* buf = NULL;
  int len = ASN1_STRING_to_UTF8(&buf, data);
  if (len <= 0)
    return false;

  value->assign(reinterpret_cast<const char*>(buf), len);
  OPENSSL_free(buf);
  return true;
}

bool ParsePrincipalValueByIndex(X509_NAME* name,
                                int index,
                                std::string* value) {
  return ParsePrincipalKeyAndValueByIndex(name, index, NULL, value);
}

bool ParsePrincipalValueByNID(X509_NAME* name, int nid, std::string* value) {
  int index = X509_NAME_get_index_by_NID(name, nid, -1);
  if (index < 0)
    return false;

  return ParsePrincipalValueByIndex(name, index, value);
}

bool ParseDate(ASN1_TIME* x509_time, base::Time* time) {
  if (!x509_time ||
      (x509_time->type != V_ASN1_UTCTIME &&
       x509_time->type != V_ASN1_GENERALIZEDTIME))
    return false;

  base::StringPiece str_date(reinterpret_cast<const char*>(x509_time->data),
                             x509_time->length);

  CertDateFormat format = x509_time->type == V_ASN1_UTCTIME ?
      CERT_DATE_FORMAT_UTC_TIME : CERT_DATE_FORMAT_GENERALIZED_TIME;
  return ParseCertificateDate(str_date, format, time);
}

}  // namespace x509_util

}  // namespace net
