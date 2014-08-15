// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "base/i18n/number_formatting.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "crypto/openssl_bio_string.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"
#include "net/base/net_util.h"
#include "net/cert/x509_util_openssl.h"
#include "ui/base/l10n/l10n_util.h"

namespace x509_util = net::x509_util;

namespace x509_certificate_model {

namespace {

std::string ProcessRawAsn1String(ASN1_STRING* data) {
  return ProcessRawBytes(ASN1_STRING_data(data), ASN1_STRING_length(data));
}

std::string ProcessRawAsn1Type(ASN1_TYPE* data) {
  int len = i2d_ASN1_TYPE(data, NULL);
  if (len <= 0)
    return std::string();

  scoped_ptr<unsigned char[]> buf(new unsigned char[len]);
  unsigned char* bufp = buf.get();

  len = i2d_ASN1_TYPE(data, &bufp);

  return ProcessRawBytes(buf.get(), len);
}

std::string ProcessRawBignum(BIGNUM* n) {
  int len = BN_num_bytes(n);
  scoped_ptr<unsigned char[]> buf(new unsigned char[len]);
  len = BN_bn2bin(n, buf.get());
  return ProcessRawBytes(buf.get(), len);
}

std::string Asn1StringToUTF8(ASN1_STRING* asn1_string) {
  std::string rv;
  unsigned char* buf = NULL;
  int len = ASN1_STRING_to_UTF8(&buf, asn1_string);
  if (len < 0)
    return rv;
  rv = std::string(reinterpret_cast<const char*>(buf), len);
  OPENSSL_free(buf);
  return rv;
}

std::string AlternativeWhenEmpty(const std::string& text,
                                 const std::string& alternative) {
  return text.empty() ? alternative : text;
}

std::string GetKeyValuesFromNameEntry(X509_NAME_ENTRY* entry) {
  std::string ret;
  std::string key;
  std::string value;
  if (!x509_util::ParsePrincipalKeyAndValue(entry, &key, &value))
    return ret;
  if (OBJ_obj2nid(X509_NAME_ENTRY_get_object(entry)) == NID_commonName)
    value = x509_certificate_model::ProcessIDN(value);
  ret = base::StringPrintf("%s = %s", key.c_str(), value.c_str());
  return ret;
}

std::string GetKeyValuesFromNameEntries(STACK_OF(X509_NAME_ENTRY)* entries) {
  std::string ret;
  size_t rdns = sk_X509_NAME_ENTRY_num(entries);
  for (size_t i = rdns - 1; i < rdns; --i) {
    X509_NAME_ENTRY* entry = sk_X509_NAME_ENTRY_value(entries, i);
    if (!entry)
      continue;
    base::StringAppendF(&ret, "%s\n", GetKeyValuesFromNameEntry(entry).c_str());
  }
  return ret;
}

std::string GetKeyValuesFromName(X509_NAME* name) {
  std::string ret;
  size_t rdns = X509_NAME_entry_count(name);
  for (size_t i = rdns - 1; i < rdns; --i) {
    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, i);
    if (!entry)
      continue;
    base::StringAppendF(&ret, "%s\n", GetKeyValuesFromNameEntry(entry).c_str());
  }
  return ret;
}

std::string Asn1ObjectToOIDString(ASN1_OBJECT* obj) {
  std::string s;
  char buf[80];
  int buflen = OBJ_obj2txt(buf, sizeof(buf), obj, 1 /* no_name */);
  if (buflen < 0)
    return s;

  s = "OID.";

  if (static_cast<size_t>(buflen) < sizeof(buf)) {
    s.append(buf, buflen);
    return s;
  }

  size_t prefix_len = s.size();
  s.resize(prefix_len + buflen + 1, ' ');
  buflen =
      OBJ_obj2txt(&s[prefix_len], s.size() - prefix_len, obj, 1 /* no_name */);
  if (buflen < 0) {
    s.clear();
    return s;
  }
  s.resize(prefix_len + buflen);
  return s;
}

int ms_cert_ext_certtype = -1;
int ms_certsrv_ca_version = -1;
int ms_ntds_replication = -1;
int eku_ms_time_stamping = -1;
int eku_ms_file_recovery = -1;
int eku_ms_windows_hardware_driver_verification = -1;
int eku_ms_qualified_subordination = -1;
int eku_ms_key_recovery = -1;
int eku_ms_document_signing = -1;
int eku_ms_lifetime_signing = -1;
int eku_ms_key_recovery_agent = -1;
int cert_attribute_ev_incorporation_country = -1;
int ns_cert_ext_ca_cert_url = -1;
int ns_cert_ext_homepage_url = -1;
int ns_cert_ext_lost_password_url = -1;
int ns_cert_ext_cert_renewal_time = -1;

int RegisterDynamicOid(const char* oid_string, const char* short_name) {
  int nid = OBJ_txt2nid(oid_string);
  if (nid > 0) {
    DVLOG(1) << "found already existing nid " << nid << " for " << oid_string;
    return nid;
  }
  return OBJ_create(oid_string, short_name, short_name);
}

class DynamicOidRegisterer {
 public:
  DynamicOidRegisterer() {
    ms_cert_ext_certtype =
        RegisterDynamicOid("1.3.6.1.4.1.311.20.2", "ms_cert_ext_certtype");
    ms_certsrv_ca_version =
        RegisterDynamicOid("1.3.6.1.4.1.311.21.1", "ms_certsrv_ca_version");
    ms_ntds_replication =
        RegisterDynamicOid("1.3.6.1.4.1.311.25.1", "ms_ntds_replication");

    eku_ms_time_stamping =
        RegisterDynamicOid("1.3.6.1.4.1.311.10.3.2", "eku_ms_time_stamping");
    eku_ms_file_recovery =
        RegisterDynamicOid("1.3.6.1.4.1.311.10.3.4.1", "eku_ms_file_recovery");
    eku_ms_windows_hardware_driver_verification =
        RegisterDynamicOid("1.3.6.1.4.1.311.10.3.5",
                           "eku_ms_windows_hardware_driver_verification");
    eku_ms_qualified_subordination = RegisterDynamicOid(
        "1.3.6.1.4.1.311.10.3.10", "eku_ms_qualified_subordination");
    eku_ms_key_recovery =
        RegisterDynamicOid("1.3.6.1.4.1.311.10.3.11", "eku_ms_key_recovery");
    eku_ms_document_signing = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.12",
                                                 "eku_ms_document_signing");
    eku_ms_lifetime_signing = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.13",
                                                 "eku_ms_lifetime_signing");
    eku_ms_key_recovery_agent =
        RegisterDynamicOid("1.3.6.1.4.1.311.21.6", "eku_ms_key_recovery_agent");

    cert_attribute_ev_incorporation_country = RegisterDynamicOid(
        "1.3.6.1.4.1.311.60.2.1.3", "cert_attribute_ev_incorporation_country");

    ns_cert_ext_ca_cert_url = RegisterDynamicOid(
        "2.16.840.1.113730.1.6", "ns_cert_ext_ca_cert_url");
    ns_cert_ext_homepage_url = RegisterDynamicOid(
        "2.16.840.1.113730.1.9", "ns_cert_ext_homepage_url");
    ns_cert_ext_lost_password_url = RegisterDynamicOid(
        "2.16.840.1.113730.1.14", "ns_cert_ext_lost_password_url");
    ns_cert_ext_cert_renewal_time = RegisterDynamicOid(
        "2.16.840.1.113730.1.15", "ns_cert_ext_cert_renewal_time");
  }
};

static base::LazyInstance<DynamicOidRegisterer>::Leaky
    g_dynamic_oid_registerer = LAZY_INSTANCE_INITIALIZER;

std::string Asn1ObjectToString(ASN1_OBJECT* obj) {
  g_dynamic_oid_registerer.Get();

  int string_id;
  int nid = OBJ_obj2nid(obj);
  switch (nid) {
    case NID_commonName:
      string_id = IDS_CERT_OID_AVA_COMMON_NAME;
      break;
    case NID_stateOrProvinceName:
      string_id = IDS_CERT_OID_AVA_STATE_OR_PROVINCE;
      break;
    case NID_organizationName:
      string_id = IDS_CERT_OID_AVA_ORGANIZATION_NAME;
      break;
    case NID_organizationalUnitName:
      string_id = IDS_CERT_OID_AVA_ORGANIZATIONAL_UNIT_NAME;
      break;
    case NID_dnQualifier:
      string_id = IDS_CERT_OID_AVA_DN_QUALIFIER;
      break;
    case NID_countryName:
      string_id = IDS_CERT_OID_AVA_COUNTRY_NAME;
      break;
    case NID_serialNumber:
      string_id = IDS_CERT_OID_AVA_SERIAL_NUMBER;
      break;
    case NID_localityName:
      string_id = IDS_CERT_OID_AVA_LOCALITY;
      break;
    case NID_domainComponent:
      string_id = IDS_CERT_OID_AVA_DC;
      break;
    case NID_rfc822Mailbox:
      string_id = IDS_CERT_OID_RFC1274_MAIL;
      break;
    case NID_userId:
      string_id = IDS_CERT_OID_RFC1274_UID;
      break;
    case NID_pkcs9_emailAddress:
      string_id = IDS_CERT_OID_PKCS9_EMAIL_ADDRESS;
      break;
    case NID_rsaEncryption:
      string_id = IDS_CERT_OID_PKCS1_RSA_ENCRYPTION;
      break;
    case NID_md2WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
      break;
    case NID_md4WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION;
      break;
    case NID_md5WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
      break;
    case NID_sha1WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
      break;
    case NID_sha256WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
      break;
    case NID_sha384WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
      break;
    case NID_sha512WithRSAEncryption:
      string_id = IDS_CERT_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
      break;
    case NID_netscape_cert_type:
      string_id = IDS_CERT_EXT_NS_CERT_TYPE;
      break;
    case NID_netscape_base_url:
      string_id = IDS_CERT_EXT_NS_CERT_BASE_URL;
      break;
    case NID_netscape_revocation_url:
      string_id = IDS_CERT_EXT_NS_CERT_REVOCATION_URL;
      break;
    case NID_netscape_ca_revocation_url:
      string_id = IDS_CERT_EXT_NS_CA_REVOCATION_URL;
      break;
    case NID_netscape_renewal_url:
      string_id = IDS_CERT_EXT_NS_CERT_RENEWAL_URL;
      break;
    case NID_netscape_ca_policy_url:
      string_id = IDS_CERT_EXT_NS_CA_POLICY_URL;
      break;
    case NID_netscape_ssl_server_name:
      string_id = IDS_CERT_EXT_NS_SSL_SERVER_NAME;
      break;
    case NID_netscape_comment:
      string_id = IDS_CERT_EXT_NS_COMMENT;
      break;
    case NID_subject_directory_attributes:
      string_id = IDS_CERT_X509_SUBJECT_DIRECTORY_ATTR;
      break;
    case NID_subject_key_identifier:
      string_id = IDS_CERT_X509_SUBJECT_KEYID;
      break;
    case NID_key_usage:
      string_id = IDS_CERT_X509_KEY_USAGE;
      break;
    case NID_subject_alt_name:
      string_id = IDS_CERT_X509_SUBJECT_ALT_NAME;
      break;
    case NID_issuer_alt_name:
      string_id = IDS_CERT_X509_ISSUER_ALT_NAME;
      break;
    case NID_basic_constraints:
      string_id = IDS_CERT_X509_BASIC_CONSTRAINTS;
      break;
    case NID_name_constraints:
      string_id = IDS_CERT_X509_NAME_CONSTRAINTS;
      break;
    case NID_crl_distribution_points:
      string_id = IDS_CERT_X509_CRL_DIST_POINTS;
      break;
    case NID_certificate_policies:
      string_id = IDS_CERT_X509_CERT_POLICIES;
      break;
    case NID_policy_mappings:
      string_id = IDS_CERT_X509_POLICY_MAPPINGS;
      break;
    case NID_policy_constraints:
      string_id = IDS_CERT_X509_POLICY_CONSTRAINTS;
      break;
    case NID_authority_key_identifier:
      string_id = IDS_CERT_X509_AUTH_KEYID;
      break;
    case NID_ext_key_usage:
      string_id = IDS_CERT_X509_EXT_KEY_USAGE;
      break;
    case NID_info_access:
      string_id = IDS_CERT_X509_AUTH_INFO_ACCESS;
      break;
    case NID_server_auth:
      string_id = IDS_CERT_EKU_TLS_WEB_SERVER_AUTHENTICATION;
      break;
    case NID_client_auth:
      string_id = IDS_CERT_EKU_TLS_WEB_CLIENT_AUTHENTICATION;
      break;
    case NID_code_sign:
      string_id = IDS_CERT_EKU_CODE_SIGNING;
      break;
    case NID_email_protect:
      string_id = IDS_CERT_EKU_EMAIL_PROTECTION;
      break;
    case NID_time_stamp:
      string_id = IDS_CERT_EKU_TIME_STAMPING;
      break;
    case NID_OCSP_sign:
      string_id = IDS_CERT_EKU_OCSP_SIGNING;
      break;
    case NID_id_qt_cps:
      string_id = IDS_CERT_PKIX_CPS_POINTER_QUALIFIER;
      break;
    case NID_id_qt_unotice:
      string_id = IDS_CERT_PKIX_USER_NOTICE_QUALIFIER;
      break;
    case NID_ms_upn:
      string_id = IDS_CERT_EXT_MS_NT_PRINCIPAL_NAME;
      break;
    case NID_ms_code_ind:
      string_id = IDS_CERT_EKU_MS_INDIVIDUAL_CODE_SIGNING;
      break;
    case NID_ms_code_com:
      string_id = IDS_CERT_EKU_MS_COMMERCIAL_CODE_SIGNING;
      break;
    case NID_ms_ctl_sign:
      string_id = IDS_CERT_EKU_MS_TRUST_LIST_SIGNING;
      break;
    case NID_ms_sgc:
      string_id = IDS_CERT_EKU_MS_SERVER_GATED_CRYPTO;
      break;
    case NID_ms_efs:
      string_id = IDS_CERT_EKU_MS_ENCRYPTING_FILE_SYSTEM;
      break;
    case NID_ms_smartcard_login:
      string_id = IDS_CERT_EKU_MS_SMART_CARD_LOGON;
      break;
    case NID_ns_sgc:
      string_id = IDS_CERT_EKU_NETSCAPE_INTERNATIONAL_STEP_UP;
      break;
    case NID_businessCategory:
      string_id = IDS_CERT_OID_BUSINESS_CATEGORY;
      break;
    case NID_undef:
      string_id = -1;
      break;

    default:
      if (nid == ms_cert_ext_certtype)
        string_id = IDS_CERT_EXT_MS_CERT_TYPE;
      else if (nid == ms_certsrv_ca_version)
        string_id = IDS_CERT_EXT_MS_CA_VERSION;
      else if (nid == ms_ntds_replication)
        string_id = IDS_CERT_EXT_MS_NTDS_REPLICATION;
      else if (nid == eku_ms_time_stamping)
        string_id = IDS_CERT_EKU_MS_TIME_STAMPING;
      else if (nid == eku_ms_file_recovery)
        string_id = IDS_CERT_EKU_MS_FILE_RECOVERY;
      else if (nid == eku_ms_windows_hardware_driver_verification)
        string_id = IDS_CERT_EKU_MS_WINDOWS_HARDWARE_DRIVER_VERIFICATION;
      else if (nid == eku_ms_qualified_subordination)
        string_id = IDS_CERT_EKU_MS_QUALIFIED_SUBORDINATION;
      else if (nid == eku_ms_key_recovery)
        string_id = IDS_CERT_EKU_MS_KEY_RECOVERY;
      else if (nid == eku_ms_document_signing)
        string_id = IDS_CERT_EKU_MS_DOCUMENT_SIGNING;
      else if (nid == eku_ms_lifetime_signing)
        string_id = IDS_CERT_EKU_MS_LIFETIME_SIGNING;
      else if (nid == eku_ms_key_recovery_agent)
        string_id = IDS_CERT_EKU_MS_KEY_RECOVERY_AGENT;
      else if (nid == cert_attribute_ev_incorporation_country)
        string_id = IDS_CERT_OID_EV_INCORPORATION_COUNTRY;
      else if (nid == ns_cert_ext_lost_password_url)
        string_id = IDS_CERT_EXT_NS_LOST_PASSWORD_URL;
      else if (nid == ns_cert_ext_cert_renewal_time)
        string_id = IDS_CERT_EXT_NS_CERT_RENEWAL_TIME;
      else
        string_id = -1;
      break;
  }
  if (string_id >= 0)
    return l10n_util::GetStringUTF8(string_id);

  return Asn1ObjectToOIDString(obj);
}

struct MaskIdPair {
  unsigned int mask;
  int string_id;
};

std::string ProcessBitField(ASN1_BIT_STRING* bitfield,
                            const MaskIdPair* string_map,
                            size_t len,
                            char separator) {
  unsigned int bits = 0;
  std::string rv;
  for (size_t i = 0;
       i < sizeof(bits) && static_cast<int>(i) < ASN1_STRING_length(bitfield);
       ++i)
    bits |= ASN1_STRING_data(bitfield)[i] << (i * 8);
  for (size_t i = 0; i < len; ++i) {
    if (bits & string_map[i].mask) {
      if (!rv.empty())
        rv += separator;
      rv += l10n_util::GetStringUTF8(string_map[i].string_id);
    }
  }
  return rv;
}

std::string ProcessNSCertTypeExtension(X509_EXTENSION* ex) {
  static const MaskIdPair usage_string_map[] = {
      {NS_SSL_CLIENT, IDS_CERT_USAGE_SSL_CLIENT},
      {NS_SSL_SERVER, IDS_CERT_USAGE_SSL_SERVER},
      {NS_SMIME, IDS_CERT_EXT_NS_CERT_TYPE_EMAIL},
      {NS_OBJSIGN, IDS_CERT_USAGE_OBJECT_SIGNER},
      {NS_SSL_CA, IDS_CERT_USAGE_SSL_CA},
      {NS_SMIME_CA, IDS_CERT_EXT_NS_CERT_TYPE_EMAIL_CA},
      {NS_OBJSIGN_CA, IDS_CERT_USAGE_OBJECT_SIGNER},
  };

  crypto::ScopedOpenSSL<ASN1_BIT_STRING, ASN1_BIT_STRING_free>::Type value(
      reinterpret_cast<ASN1_BIT_STRING*>(X509V3_EXT_d2i(ex)));
  if (!value.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  return ProcessBitField(value.get(),
                         usage_string_map,
                         ARRAYSIZE_UNSAFE(usage_string_map),
                         '\n');
}

std::string ProcessKeyUsageExtension(X509_EXTENSION* ex) {
  static const MaskIdPair key_usage_string_map[] = {
      {KU_DIGITAL_SIGNATURE, IDS_CERT_X509_KEY_USAGE_SIGNING},
      {KU_NON_REPUDIATION, IDS_CERT_X509_KEY_USAGE_NONREP},
      {KU_KEY_ENCIPHERMENT, IDS_CERT_X509_KEY_USAGE_ENCIPHERMENT},
      {KU_DATA_ENCIPHERMENT, IDS_CERT_X509_KEY_USAGE_DATA_ENCIPHERMENT},
      {KU_KEY_AGREEMENT, IDS_CERT_X509_KEY_USAGE_KEY_AGREEMENT},
      {KU_KEY_CERT_SIGN, IDS_CERT_X509_KEY_USAGE_CERT_SIGNER},
      {KU_CRL_SIGN, IDS_CERT_X509_KEY_USAGE_CRL_SIGNER},
      {KU_ENCIPHER_ONLY, IDS_CERT_X509_KEY_USAGE_ENCIPHER_ONLY},
      {KU_DECIPHER_ONLY, IDS_CERT_X509_KEY_USAGE_DECIPHER_ONLY},
  };

  crypto::ScopedOpenSSL<ASN1_BIT_STRING, ASN1_BIT_STRING_free>::Type value(
      reinterpret_cast<ASN1_BIT_STRING*>(X509V3_EXT_d2i(ex)));
  if (!value.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  return ProcessBitField(value.get(),
                         key_usage_string_map,
                         ARRAYSIZE_UNSAFE(key_usage_string_map),
                         '\n');
}

std::string ProcessBasicConstraints(X509_EXTENSION* ex) {
  std::string rv;
  crypto::ScopedOpenSSL<BASIC_CONSTRAINTS, BASIC_CONSTRAINTS_free>::Type value(
      reinterpret_cast<BASIC_CONSTRAINTS*>(X509V3_EXT_d2i(ex)));
  if (!value.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  if (value.get()->ca)
    rv = l10n_util::GetStringUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_IS_CA);
  else
    rv = l10n_util::GetStringUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_IS_NOT_CA);
  rv += '\n';
  if (value.get()->ca) {
    base::string16 depth;
    if (!value.get()->pathlen) {
      depth = l10n_util::GetStringUTF16(
          IDS_CERT_X509_BASIC_CONSTRAINT_PATH_LEN_UNLIMITED);
    } else {
      depth = base::FormatNumber(ASN1_INTEGER_get(value.get()->pathlen));
    }
    rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_PATH_LEN,
                                    depth);
  }
  return rv;
}

std::string ProcessExtKeyUsage(X509_EXTENSION* ex) {
  std::string rv;
  crypto::ScopedOpenSSL<EXTENDED_KEY_USAGE, EXTENDED_KEY_USAGE_free>::Type
      value(reinterpret_cast<EXTENDED_KEY_USAGE*>(X509V3_EXT_d2i(ex)));
  if (!value.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  for (size_t i = 0; i < sk_ASN1_OBJECT_num(value.get()); i++) {
    ASN1_OBJECT* obj = sk_ASN1_OBJECT_value(value.get(), i);
    std::string oid_dump = Asn1ObjectToOIDString(obj);
    std::string oid_text = Asn1ObjectToString(obj);

    // If oid is one we recognize, oid_text will have a text description of the
    // OID, which we display along with the oid_dump.  If we don't recognize the
    // OID, they will be the same, so just display the OID alone.
    if (oid_dump == oid_text)
      rv += oid_dump;
    else
      rv += l10n_util::GetStringFUTF8(IDS_CERT_EXT_KEY_USAGE_FORMAT,
                                      base::UTF8ToUTF16(oid_text),
                                      base::UTF8ToUTF16(oid_dump));
    rv += '\n';
  }
  return rv;
}

std::string ProcessGeneralName(GENERAL_NAME* name) {
  std::string key;
  std::string value;

  switch (name->type) {
    case GEN_OTHERNAME: {
      ASN1_OBJECT* oid;
      ASN1_TYPE* asn1_value;
      GENERAL_NAME_get0_otherName(name, &oid, &asn1_value);
      key = Asn1ObjectToString(oid);
      // g_dynamic_oid_registerer.Get() will have been run by
      // Asn1ObjectToString.
      int nid = OBJ_obj2nid(oid);
      if (nid == IDS_CERT_EXT_MS_NT_PRINCIPAL_NAME) {
        // The type of this name is apparently nowhere explicitly
        // documented. However, in the generated templates, it is always
        // UTF-8. So try to decode this as UTF-8; if that fails, dump the
        // raw data.
        if (asn1_value->type == V_ASN1_UTF8STRING) {
          value = std::string(reinterpret_cast<char*>(ASN1_STRING_data(
                                  asn1_value->value.utf8string)),
                              ASN1_STRING_length(asn1_value->value.utf8string));
        } else {
          value = ProcessRawAsn1Type(asn1_value);
        }
      } else if (nid == ms_ntds_replication) {
        // This should be a 16-byte GUID.
        if (asn1_value->type == V_ASN1_OCTET_STRING &&
            asn1_value->value.octet_string->length == 16) {
          unsigned char* d = asn1_value->value.octet_string->data;
          base::SStringPrintf(
              &value,
              "{%.2x%.2x%.2x%.2x-%.2x%.2x-%.2x%.2x-"
              "%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}",
              d[3], d[2], d[1], d[0], d[5], d[4], d[7], d[6],
              d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        } else {
          value = ProcessRawAsn1Type(asn1_value);
        }
      } else {
        value = ProcessRawAsn1Type(asn1_value);
      }
      break;
    }
    case GEN_EMAIL:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_RFC822_NAME);
      value = std::string(
          reinterpret_cast<char*>(ASN1_STRING_data(name->d.rfc822Name)),
          ASN1_STRING_length(name->d.rfc822Name));
      break;
    case GEN_DNS:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_DNS_NAME);
      value = std::string(
          reinterpret_cast<char*>(ASN1_STRING_data(name->d.dNSName)),
          ASN1_STRING_length(name->d.dNSName));
      value = ProcessIDN(value);
      break;
    case GEN_X400:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_X400_ADDRESS);
      value = ProcessRawAsn1Type(name->d.x400Address);
      break;
    case GEN_DIRNAME:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_DIRECTORY_NAME);
      value = GetKeyValuesFromName(name->d.directoryName);
      break;
    case GEN_EDIPARTY:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_EDI_PARTY_NAME);
      if (name->d.ediPartyName->nameAssigner &&
          ASN1_STRING_length(name->d.ediPartyName->nameAssigner) > 0) {
        value += l10n_util::GetStringFUTF8(
            IDS_CERT_EDI_NAME_ASSIGNER,
            base::UTF8ToUTF16(
                Asn1StringToUTF8(name->d.ediPartyName->nameAssigner)));
        value += "\n";
      }
      if (name->d.ediPartyName->partyName &&
          ASN1_STRING_length(name->d.ediPartyName->partyName) > 0) {
        value += l10n_util::GetStringFUTF8(
            IDS_CERT_EDI_PARTY_NAME,
            base::UTF8ToUTF16(
                Asn1StringToUTF8(name->d.ediPartyName->partyName)));
        value += "\n";
      }
      break;
    case GEN_URI:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_URI);
      value =
          std::string(reinterpret_cast<char*>(
                          ASN1_STRING_data(name->d.uniformResourceIdentifier)),
                      ASN1_STRING_length(name->d.uniformResourceIdentifier));
      break;
    case GEN_IPADD: {
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_IP_ADDRESS);
      net::IPAddressNumber ip(ASN1_STRING_data(name->d.iPAddress),
                              ASN1_STRING_data(name->d.iPAddress) +
                                  ASN1_STRING_length(name->d.iPAddress));
      if (net::GetAddressFamily(ip) != net::ADDRESS_FAMILY_UNSPECIFIED) {
        value = net::IPAddressToString(ip);
      } else {
        // Invalid IP address.
        value = ProcessRawBytes(ip.data(), ip.size());
      }
      break;
    }
    case GEN_RID:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_REGISTERED_ID);
      value = Asn1ObjectToString(name->d.registeredID);
      break;
  }
  std::string rv(l10n_util::GetStringFUTF8(IDS_CERT_UNKNOWN_OID_INFO_FORMAT,
                                           base::UTF8ToUTF16(key),
                                           base::UTF8ToUTF16(value)));
  rv += '\n';
  return rv;
}

std::string ProcessGeneralNames(GENERAL_NAMES* names) {
  std::string rv;
  for (size_t i = 0; i < sk_GENERAL_NAME_num(names); ++i) {
    GENERAL_NAME* name = sk_GENERAL_NAME_value(names, i);
    rv += ProcessGeneralName(name);
  }
  return rv;
}

std::string ProcessAltName(X509_EXTENSION* ex) {
  crypto::ScopedOpenSSL<GENERAL_NAMES, GENERAL_NAMES_free>::Type alt_names(
      reinterpret_cast<GENERAL_NAMES*>(X509V3_EXT_d2i(ex)));
  if (!alt_names.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  return ProcessGeneralNames(alt_names.get());
}

std::string ProcessSubjectKeyId(X509_EXTENSION* ex) {
  crypto::ScopedOpenSSL<ASN1_OCTET_STRING, ASN1_OCTET_STRING_free>::Type value(
      reinterpret_cast<ASN1_OCTET_STRING*>(X509V3_EXT_d2i(ex)));
  if (!value.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  return l10n_util::GetStringFUTF8(
      IDS_CERT_KEYID_FORMAT,
      base::ASCIIToUTF16(ProcessRawAsn1String(value.get())));
}

std::string ProcessAuthKeyId(X509_EXTENSION* ex) {
  std::string rv;
  crypto::ScopedOpenSSL<AUTHORITY_KEYID, AUTHORITY_KEYID_free>::Type value(
      reinterpret_cast<AUTHORITY_KEYID*>(X509V3_EXT_d2i(ex)));
  if (!value.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  if (value.get()->keyid && ASN1_STRING_length(value.get()->keyid) > 0) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_KEYID_FORMAT,
        base::ASCIIToUTF16(ProcessRawAsn1String(value.get()->keyid)));
    rv += '\n';
  }

  if (value.get()->issuer) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_ISSUER_FORMAT,
        base::UTF8ToUTF16(ProcessGeneralNames(value.get()->issuer)));
    rv += '\n';
  }

  if (value.get()->serial) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_SERIAL_NUMBER_FORMAT,
        base::ASCIIToUTF16(ProcessRawAsn1String(value.get()->serial)));
    rv += '\n';
  }

  return rv;
}

std::string ProcessUserNotice(USERNOTICE* notice) {
  std::string rv;
  if (notice->noticeref) {
    rv += Asn1StringToUTF8(notice->noticeref->organization);
    rv += " - ";
    for (size_t i = 0; i < sk_ASN1_INTEGER_num(notice->noticeref->noticenos);
         ++i) {
      ASN1_INTEGER* info =
          sk_ASN1_INTEGER_value(notice->noticeref->noticenos, i);
      long number = ASN1_INTEGER_get(info);
      if (number != -1) {
        if (i != sk_ASN1_INTEGER_num(notice->noticeref->noticenos) - 1)
          rv += ", ";
        rv += '#';
        rv += base::IntToString(number);
      }
    }
  }
  if (notice->exptext && notice->exptext->length != 0) {
    rv += "\n    ";
    rv += Asn1StringToUTF8(notice->exptext);
  }
  return rv;
}

std::string ProcessCertificatePolicies(X509_EXTENSION* ex) {
  std::string rv;
  crypto::ScopedOpenSSL<CERTIFICATEPOLICIES, CERTIFICATEPOLICIES_free>::Type
      policies(reinterpret_cast<CERTIFICATEPOLICIES*>(X509V3_EXT_d2i(ex)));

  if (!policies.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  for (size_t i = 0; i < sk_POLICYINFO_num(policies.get()); ++i) {
    POLICYINFO* info = sk_POLICYINFO_value(policies.get(), i);
    std::string key = Asn1ObjectToString(info->policyid);
    // If we have policy qualifiers, display the oid text
    // with a ':', otherwise just put the oid text and a newline.
    if (info->qualifiers && sk_POLICYQUALINFO_num(info->qualifiers)) {
      rv += l10n_util::GetStringFUTF8(IDS_CERT_MULTILINE_INFO_START_FORMAT,
                                      base::UTF8ToUTF16(key));
    } else {
      rv += key;
    }
    rv += '\n';

    if (info->qualifiers && sk_POLICYQUALINFO_num(info->qualifiers)) {
      // Add all qualifiers on separate lines, indented.
      for (size_t i = 0; i < sk_POLICYQUALINFO_num(info->qualifiers); ++i) {
        POLICYQUALINFO* qualifier =
            sk_POLICYQUALINFO_value(info->qualifiers, i);
        rv += "  ";
        rv += l10n_util::GetStringFUTF8(
            IDS_CERT_MULTILINE_INFO_START_FORMAT,
            base::UTF8ToUTF16(Asn1ObjectToString(qualifier->pqualid)));
        int nid = OBJ_obj2nid(qualifier->pqualid);
        switch (nid) {
          case NID_id_qt_cps:
            rv += "    ";
            rv += std::string(
                reinterpret_cast<char*>(ASN1_STRING_data(qualifier->d.cpsuri)),
                ASN1_STRING_length(qualifier->d.cpsuri));
            break;
          case NID_id_qt_unotice:
            rv += ProcessUserNotice(qualifier->d.usernotice);
            break;
          default:
            rv += ProcessRawAsn1Type(qualifier->d.other);
            break;
        }
        rv += '\n';
      }
    }
  }
  return rv;
}

std::string ProcessCrlDistPoints(X509_EXTENSION* ex) {
  static const MaskIdPair reason_string_map[] = {
      // OpenSSL doesn't define contants for these.  (The CRL_REASON_ defines in
      // x509v3.h are for the "X509v3 CRL Reason Code" extension.)
      // These are from RFC5280 section 4.2.1.13.
      {0, IDS_CERT_REVOCATION_REASON_UNUSED},
      {1, IDS_CERT_REVOCATION_REASON_KEY_COMPROMISE},
      {2, IDS_CERT_REVOCATION_REASON_CA_COMPROMISE},
      {3, IDS_CERT_REVOCATION_REASON_AFFILIATION_CHANGED},
      {4, IDS_CERT_REVOCATION_REASON_SUPERSEDED},
      {5, IDS_CERT_REVOCATION_REASON_CESSATION_OF_OPERATION},
      {6, IDS_CERT_REVOCATION_REASON_CERTIFICATE_HOLD},
      {7, IDS_CERT_REVOCATION_REASON_PRIVILEGE_WITHDRAWN},
      {8, IDS_CERT_REVOCATION_REASON_AA_COMPROMISE},
  };
  // OpenSSL doesn't define constants for the DIST_POINT type field. These
  // values are from reading openssl/crypto/x509v3/v3_crld.c
  const int kDistPointFullName = 0;
  const int kDistPointRelativeName = 1;

  std::string rv;
  crypto::ScopedOpenSSL<CRL_DIST_POINTS, CRL_DIST_POINTS_free>::Type
      dist_points(reinterpret_cast<CRL_DIST_POINTS*>(X509V3_EXT_d2i(ex)));

  if (!dist_points.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  for (size_t i = 0; i < sk_DIST_POINT_num(dist_points.get()); ++i) {
    DIST_POINT* point = sk_DIST_POINT_value(dist_points.get(), i);
    if (point->distpoint) {
      switch (point->distpoint->type) {
        case kDistPointFullName:
          rv += ProcessGeneralNames(point->distpoint->name.fullname);
          break;
        case kDistPointRelativeName:
          rv +=
              GetKeyValuesFromNameEntries(point->distpoint->name.relativename);
          // TODO(mattm): should something be done with
          // point->distpoint->dpname?
          break;
      }
    }
    if (point->reasons) {
      rv += ' ';
      rv += ProcessBitField(point->reasons,
                            reason_string_map,
                            ARRAYSIZE_UNSAFE(reason_string_map),
                            ',');
      rv += '\n';
    }
    if (point->CRLissuer) {
      rv += l10n_util::GetStringFUTF8(
          IDS_CERT_ISSUER_FORMAT,
          base::UTF8ToUTF16(ProcessGeneralNames(point->CRLissuer)));
    }
  }

  return rv;
}

std::string ProcessAuthInfoAccess(X509_EXTENSION* ex) {
  std::string rv;
  crypto::ScopedOpenSSL<AUTHORITY_INFO_ACCESS, AUTHORITY_INFO_ACCESS_free>::Type
      aia(reinterpret_cast<AUTHORITY_INFO_ACCESS*>(X509V3_EXT_d2i(ex)));

  if (!aia.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  for (size_t i = 0; i < sk_ACCESS_DESCRIPTION_num(aia.get()); ++i) {
    ACCESS_DESCRIPTION* desc = sk_ACCESS_DESCRIPTION_value(aia.get(), i);

    base::string16 location_str =
        base::UTF8ToUTF16(ProcessGeneralName(desc->location));
    switch (OBJ_obj2nid(desc->method)) {
      case NID_ad_OCSP:
        rv += l10n_util::GetStringFUTF8(IDS_CERT_OCSP_RESPONDER_FORMAT,
                                        location_str);
        break;
      case NID_ad_ca_issuers:
        rv +=
            l10n_util::GetStringFUTF8(IDS_CERT_CA_ISSUERS_FORMAT, location_str);
        break;
      default:
        rv += l10n_util::GetStringFUTF8(
            IDS_CERT_UNKNOWN_OID_INFO_FORMAT,
            base::UTF8ToUTF16(Asn1ObjectToString(desc->method)),
            location_str);
        break;
    }
  }
  return rv;
}

std::string ProcessIA5StringData(ASN1_OCTET_STRING* asn1_string) {
  const unsigned char* data = ASN1_STRING_data(asn1_string);
  crypto::ScopedOpenSSL<ASN1_IA5STRING, ASN1_IA5STRING_free>::Type ia5_string(
      d2i_ASN1_IA5STRING(NULL, &data, ASN1_STRING_length(asn1_string)));

  if (!ia5_string.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  return std::string(
      reinterpret_cast<char*>(ASN1_STRING_data(ia5_string.get())),
      ASN1_STRING_length(ia5_string.get()));
}

std::string ProcessBMPStringData(ASN1_OCTET_STRING* asn1_string) {
  const unsigned char* data = ASN1_STRING_data(asn1_string);
  crypto::ScopedOpenSSL<ASN1_BMPSTRING, ASN1_BMPSTRING_free>::Type bmp_string(
      d2i_ASN1_BMPSTRING(NULL, &data, ASN1_STRING_length(asn1_string)));

  if (!bmp_string.get())
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  return Asn1StringToUTF8(bmp_string.get());
}

std::string X509ExtensionValueToString(X509_EXTENSION* ex) {
  g_dynamic_oid_registerer.Get();
  int nid = OBJ_obj2nid(X509_EXTENSION_get_object(ex));
  switch (nid) {
    case NID_netscape_cert_type:
      return ProcessNSCertTypeExtension(ex);
    case NID_key_usage:
      return ProcessKeyUsageExtension(ex);
    case NID_basic_constraints:
      return ProcessBasicConstraints(ex);
    case NID_ext_key_usage:
      return ProcessExtKeyUsage(ex);
    case NID_issuer_alt_name:
    case NID_subject_alt_name:
      return ProcessAltName(ex);
    case NID_subject_key_identifier:
      return ProcessSubjectKeyId(ex);
    case NID_authority_key_identifier:
      return ProcessAuthKeyId(ex);
    case NID_certificate_policies:
      return ProcessCertificatePolicies(ex);
    case NID_crl_distribution_points:
      return ProcessCrlDistPoints(ex);
    case NID_info_access:
      return ProcessAuthInfoAccess(ex);
    case NID_netscape_base_url:
    case NID_netscape_revocation_url:
    case NID_netscape_ca_revocation_url:
    case NID_netscape_renewal_url:
    case NID_netscape_ca_policy_url:
    case NID_netscape_comment:
    case NID_netscape_ssl_server_name:
      return ProcessIA5StringData(X509_EXTENSION_get_data(ex));
    default:
      if (nid == ns_cert_ext_ca_cert_url ||
          nid == ns_cert_ext_homepage_url ||
          nid == ns_cert_ext_lost_password_url)
        return ProcessIA5StringData(X509_EXTENSION_get_data(ex));
      if (nid == ms_cert_ext_certtype)
        return ProcessBMPStringData(X509_EXTENSION_get_data(ex));
      return ProcessRawAsn1String(X509_EXTENSION_get_data(ex));
  }
}

}  // namespace

using net::X509Certificate;

std::string GetCertNameOrNickname(X509Certificate::OSCertHandle cert_handle) {
  std::string name =
      ProcessIDN(GetSubjectCommonName(cert_handle, std::string()));
  if (!name.empty())
    return name;

  crypto::ScopedBIO bio(crypto::BIO_new_string(&name));
  if (!bio.get())
    return name;
  X509_NAME_print_ex(bio.get(),
                     X509_get_subject_name(cert_handle),
                     0 /* indent */,
                     XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB);
  return name;
}

std::string GetTokenName(X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetVersion(net::X509Certificate::OSCertHandle cert_handle) {
  unsigned long version = X509_get_version(cert_handle);
  if (version != ULONG_MAX)
    return base::UintToString(version + 1);
  return "";
}

net::CertType GetType(X509Certificate::OSCertHandle os_cert) {
  // TODO(bulach): implement me.
  return net::OTHER_CERT;
}

void GetUsageStrings(X509Certificate::OSCertHandle cert_handle,
                         std::vector<std::string>* usages) {
  // TODO(bulach): implement me.
}

std::string GetSerialNumberHexified(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  ASN1_INTEGER* num = X509_get_serialNumber(cert_handle);
  const char kSerialNumberSeparator = ':';
  std::string hex_string = ProcessRawBytesWithSeparators(
      num->data, num->length, kSerialNumberSeparator, kSerialNumberSeparator);
  return AlternativeWhenEmpty(hex_string, alternative_text);
}

std::string GetIssuerCommonName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  x509_util::ParsePrincipalValueByNID(X509_get_issuer_name(cert_handle),
                                      NID_commonName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetIssuerOrgName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  x509_util::ParsePrincipalValueByNID(X509_get_issuer_name(cert_handle),
                                      NID_organizationName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetIssuerOrgUnitName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  x509_util::ParsePrincipalValueByNID(X509_get_issuer_name(cert_handle),
                                      NID_organizationalUnitName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetSubjectOrgName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  x509_util::ParsePrincipalValueByNID(X509_get_subject_name(cert_handle),
                                      NID_organizationName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetSubjectOrgUnitName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  x509_util::ParsePrincipalValueByNID(X509_get_subject_name(cert_handle),
                                      NID_organizationalUnitName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetSubjectCommonName(X509Certificate::OSCertHandle cert_handle,
                                 const std::string& alternative_text) {
  std::string ret;
  x509_util::ParsePrincipalValueByNID(X509_get_subject_name(cert_handle),
                                      NID_commonName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

bool GetTimes(X509Certificate::OSCertHandle cert_handle,
              base::Time* issued, base::Time* expires) {
  return x509_util::ParseDate(X509_get_notBefore(cert_handle), issued) &&
         x509_util::ParseDate(X509_get_notAfter(cert_handle), expires);
}

std::string GetTitle(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(mattm): merge GetTitle and GetCertNameOrNickname?
  // Is there any reason GetCertNameOrNickname calls ProcessIDN and this
  // doesn't?
  std::string title =
      GetSubjectCommonName(cert_handle, std::string());
  if (!title.empty())
    return title;

  crypto::ScopedBIO bio(crypto::BIO_new_string(&title));
  if (!bio.get())
    return title;
  X509_NAME_print_ex(bio.get(),
                     X509_get_subject_name(cert_handle),
                     0 /* indent */,
                     XN_FLAG_RFC2253 & ~ASN1_STRFLGS_ESC_MSB);
  return title;
}

std::string GetIssuerName(net::X509Certificate::OSCertHandle cert_handle) {
  return GetKeyValuesFromName(X509_get_issuer_name(cert_handle));
}

std::string GetSubjectName(net::X509Certificate::OSCertHandle cert_handle) {
  return GetKeyValuesFromName(X509_get_subject_name(cert_handle));
}

void GetExtensions(
    const std::string& critical_label,
    const std::string& non_critical_label,
    net::X509Certificate::OSCertHandle cert_handle,
    Extensions* extensions) {
  for (int i = 0; i < X509_get_ext_count(cert_handle); ++i) {
    X509_EXTENSION* ex = X509_get_ext(cert_handle, i);
    ASN1_OBJECT* obj = X509_EXTENSION_get_object(ex);

    Extension extension;
    extension.name = Asn1ObjectToString(obj);
    extension.value = (X509_EXTENSION_get_critical(ex) ? critical_label
                                                       : non_critical_label) +
                      "\n" + X509ExtensionValueToString(ex);
    extensions->push_back(extension);
  }
}

std::string HashCertSHA256(net::X509Certificate::OSCertHandle cert_handle) {
  unsigned char sha256_data[SHA256_DIGEST_LENGTH] = {0};
  unsigned int sha256_size = sizeof(sha256_data);
  int ret = X509_digest(cert_handle, EVP_sha256(), sha256_data, &sha256_size);
  DCHECK(ret);
  DCHECK_EQ(sha256_size, sizeof(sha256_data));
  return ProcessRawBytes(sha256_data, sha256_size);
}

std::string HashCertSHA1(net::X509Certificate::OSCertHandle cert_handle) {
  unsigned char sha1_data[SHA_DIGEST_LENGTH] = {0};
  unsigned int sha1_size = sizeof(sha1_data);
  int ret = X509_digest(cert_handle, EVP_sha1(), sha1_data, &sha1_size);
  DCHECK(ret);
  DCHECK_EQ(sha1_size, sizeof(sha1_data));
  return ProcessRawBytes(sha1_data, sha1_size);
}

std::string GetCMSString(const net::X509Certificate::OSCertHandles& cert_chain,
                         size_t start, size_t end) {
  STACK_OF(X509)* certs = sk_X509_new_null();

  for (size_t i = start; i < end; ++i) {
    sk_X509_push(certs, cert_chain[i]);
  }

  CBB pkcs7;
  CBB_init(&pkcs7, 1024 * sk_X509_num(certs));

  uint8_t *pkcs7_data;
  size_t pkcs7_len;
  if (!PKCS7_bundle_certificates(&pkcs7, certs) ||
      !CBB_finish(&pkcs7, &pkcs7_data, &pkcs7_len)) {
    CBB_cleanup(&pkcs7);
    sk_X509_free(certs);
    return "";
  }

  std::string ret(reinterpret_cast<char*>(pkcs7_data), pkcs7_len);
  OPENSSL_free(pkcs7_data);
  sk_X509_free(certs);

  return ret;
}

std::string ProcessSecAlgorithmSignature(
    net::X509Certificate::OSCertHandle cert_handle) {
  return Asn1ObjectToString(cert_handle->cert_info->signature->algorithm);
}

std::string ProcessSecAlgorithmSubjectPublicKey(
    net::X509Certificate::OSCertHandle cert_handle) {
  return Asn1ObjectToString(
      X509_get_X509_PUBKEY(cert_handle)->algor->algorithm);
}

std::string ProcessSecAlgorithmSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle) {
  return Asn1ObjectToString(cert_handle->sig_alg->algorithm);
}

std::string ProcessSubjectPublicKeyInfo(
    net::X509Certificate::OSCertHandle cert_handle) {
  std::string rv;
  crypto::ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free>::Type public_key(
      X509_get_pubkey(cert_handle));
  if (!public_key.get())
    return rv;
  switch (EVP_PKEY_type(public_key.get()->type)) {
    case EVP_PKEY_RSA: {
      crypto::ScopedOpenSSL<RSA, RSA_free>::Type rsa_key(
          EVP_PKEY_get1_RSA(public_key.get()));
      if (!rsa_key.get())
        return rv;
      rv = l10n_util::GetStringFUTF8(
          IDS_CERT_RSA_PUBLIC_KEY_DUMP_FORMAT,
          base::UintToString16(BN_num_bits(rsa_key.get()->n)),
          base::UTF8ToUTF16(ProcessRawBignum(rsa_key.get()->n)),
          base::UintToString16(BN_num_bits(rsa_key.get()->e)),
          base::UTF8ToUTF16(ProcessRawBignum(rsa_key.get()->e)));
      return rv;
    }
    default:
      rv = ProcessRawAsn1String(X509_get_X509_PUBKEY(cert_handle)->public_key);
      return rv;
  }
}

std::string ProcessRawBitsSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle) {
  return ProcessRawAsn1String(cert_handle->signature);
}

}  // namespace x509_certificate_model
