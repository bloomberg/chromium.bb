// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "net/base/x509_openssl_util.h"

namespace nxou = net::x509_openssl_util;

namespace {

std::string AlternativeWhenEmpty(const std::string& text,
                                 const std::string& alternative) {
  return text.empty() ? alternative : text;
}

std::string GetKeyValuesFromName(X509_NAME* name) {
  std::string ret;
  int rdns = X509_NAME_entry_count(name) - 1;
  for (int i = rdns; i >= 0; --i) {
    std::string key;
    std::string value;
    if (!nxou::ParsePrincipalKeyAndValueByIndex(name, i, &key, &value))
      break;
    ret += key;
    ret += " = ";
    ret += value;
    ret += '\n';
  }
  return ret;
}

}  // namepsace

namespace x509_certificate_model {

using net::X509Certificate;

std::string GetCertNameOrNickname(X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetNickname(X509Certificate::OSCertHandle cert_handle) {
  // TODO(jamescook): implement me.
  return "";
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
  return net::UNKNOWN_CERT;
}

std::string GetEmailAddress(X509Certificate::OSCertHandle os_cert) {
  // TODO(bulach): implement me.
  return "";
}

void GetUsageStrings(X509Certificate::OSCertHandle cert_handle,
                         std::vector<std::string>* usages) {
  // TODO(bulach): implement me.
}

std::string GetKeyUsageString(X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
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
  nxou::ParsePrincipalValueByNID(X509_get_issuer_name(cert_handle),
                                 NID_commonName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetIssuerOrgName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  nxou::ParsePrincipalValueByNID(X509_get_issuer_name(cert_handle),
                                 NID_organizationName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetIssuerOrgUnitName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  nxou::ParsePrincipalValueByNID(X509_get_issuer_name(cert_handle),
                                 NID_organizationalUnitName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetSubjectOrgName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  nxou::ParsePrincipalValueByNID(X509_get_subject_name(cert_handle),
                                 NID_organizationName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetSubjectOrgUnitName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  std::string ret;
  nxou::ParsePrincipalValueByNID(X509_get_subject_name(cert_handle),
                                 NID_organizationalUnitName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

std::string GetSubjectCommonName(X509Certificate::OSCertHandle cert_handle,
                                 const std::string& alternative_text) {
  std::string ret;
  nxou::ParsePrincipalValueByNID(X509_get_subject_name(cert_handle),
                                 NID_commonName, &ret);
  return AlternativeWhenEmpty(ret, alternative_text);
}

bool GetTimes(X509Certificate::OSCertHandle cert_handle,
              base::Time* issued, base::Time* expires) {
  return nxou::ParseDate(X509_get_notBefore(cert_handle), issued) &&
         nxou::ParseDate(X509_get_notAfter(cert_handle), expires);
}

std::string GetTitle(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetIssuerName(net::X509Certificate::OSCertHandle cert_handle) {
  return GetKeyValuesFromName(X509_get_issuer_name(cert_handle));
}

std::string GetSubjectName(net::X509Certificate::OSCertHandle cert_handle) {
  return GetKeyValuesFromName(X509_get_subject_name(cert_handle));
}

void GetEmailAddresses(net::X509Certificate::OSCertHandle cert_handle,
                       std::vector<std::string>* email_addresses) {
  // TODO(bulach): implement me.
}

void GetNicknameStringsFromCertList(
    const std::vector<scoped_refptr<net::X509Certificate> >& certs,
    const std::string& cert_expired,
    const std::string& cert_not_yet_valid,
    std::vector<std::string>* nick_names) {
  // TODO(bulach): implement me.
}

std::string GetPkcs11Id(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(jamescook): implement me.
  return "";
}

void GetExtensions(
    const std::string& critical_label,
    const std::string& non_critical_label,
    net::X509Certificate::OSCertHandle cert_handle,
    Extensions* extensions) {
  // TODO(bulach): implement me.
}

std::string HashCertSHA256(net::X509Certificate::OSCertHandle cert_handle) {
  unsigned char sha256_data[SHA256_DIGEST_LENGTH] = {0};
  unsigned int sha256_size = sizeof(sha256_data);
  int ret = X509_digest(cert_handle, EVP_sha256(), sha256_data, &sha256_size);
  CHECK(ret);
  CHECK_EQ(sha256_size, sizeof(sha256_data));
  return ProcessRawBytes(sha256_data, sha256_size);
}

std::string HashCertSHA1(net::X509Certificate::OSCertHandle cert_handle) {
  unsigned char sha1_data[SHA_DIGEST_LENGTH] = {0};
  unsigned int sha1_size = sizeof(sha1_data);
  int ret = X509_digest(cert_handle, EVP_sha1(), sha1_data, &sha1_size);
  CHECK(ret);
  CHECK_EQ(sha1_size, sizeof(sha1_data));
  return ProcessRawBytes(sha1_data, sha1_size);
}

void GetCertChainFromCert(net::X509Certificate::OSCertHandle cert_handle,
                          net::X509Certificate::OSCertHandles* cert_handles) {
  // TODO(bulach): how to get the chain out of a certificate?
  cert_handles->push_back(net::X509Certificate::DupOSCertHandle(cert_handle));
}

void DestroyCertChain(net::X509Certificate::OSCertHandles* cert_handles) {
  for (net::X509Certificate::OSCertHandles::iterator i = cert_handles->begin();
       i != cert_handles->end(); ++i)
    X509_free(*i);
  cert_handles->clear();
}

std::string GetDerString(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetCMSString(const net::X509Certificate::OSCertHandles& cert_chain,
                         size_t start, size_t end) {
  // TODO(bulach): implement me.
  return "";
}

std::string ProcessSecAlgorithmSignature(
    net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string ProcessSecAlgorithmSubjectPublicKey(
    net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string ProcessSecAlgorithmSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string ProcessSubjectPublicKeyInfo(
    net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string ProcessRawBitsSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

void RegisterDynamicOids() {
}

}  // namespace x509_certificate_model
