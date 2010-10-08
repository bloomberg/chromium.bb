// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/x509v3.h>

#include "chrome/common/net/x509_certificate_model.h"

#include "net/base/x509_certificate.h"

namespace x509_certificate_model {

using net::X509Certificate;

std::string GetCertNameOrNickname(X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetTokenName(X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetVersion(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
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
  // TODO(bulach): implement me.
  return "";
}

std::string GetIssuerCommonName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetIssuerOrgName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetIssuerOrgUnitName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetSubjectOrgName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetSubjectOrgUnitName(
    X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetSubjectCommonName(X509Certificate::OSCertHandle cert_handle,
                                 const std::string& alternative_text) {
  // TODO(bulach): implement me.
  return "";
}

bool GetTimes(X509Certificate::OSCertHandle cert_handle,
              base::Time* issued, base::Time* expires) {
  // TODO(bulach): implement me.
  return false;
}

std::string GetTitle(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetIssuerName(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string GetSubjectName(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
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

void GetExtensions(
    const std::string& critical_label,
    const std::string& non_critical_label,
    net::X509Certificate::OSCertHandle cert_handle,
    Extensions* extensions) {
  // TODO(bulach): implement me.
}

std::string HashCertSHA256(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

std::string HashCertSHA1(net::X509Certificate::OSCertHandle cert_handle) {
  // TODO(bulach): implement me.
  return "";
}

void GetCertChainFromCert(net::X509Certificate::OSCertHandle cert_handle,
                          net::X509Certificate::OSCertHandles* cert_handles) {
  // TODO(bulach): implement me.
}

void DestroyCertChain(net::X509Certificate::OSCertHandles* cert_handles) {
  // TODO(bulach): implement me.
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
