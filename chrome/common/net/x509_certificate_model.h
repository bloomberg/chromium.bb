// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_X509_CERTIFICATE_MODEL_H_
#define NET_BASE_X509_CERTIFICATE_MODEL_H_
#pragma once

#include "net/base/cert_database.h"
#include "net/base/cert_type.h"
#include "net/base/x509_certificate.h"

// This namespace defines a set of functions to be used in UI-related bits of
// X509 certificates. It decouples the UI from the underlying crypto library
// (currently NSS or OpenSSL - in development).
// This is currently only used by linux, as mac / windows use their own native
// certificate viewers and crypto libraries.
namespace x509_certificate_model {

std::string GetCertNameOrNickname(
    net::X509Certificate::OSCertHandle cert_handle);

std::string GetTokenName(net::X509Certificate::OSCertHandle cert_handle);

std::string GetVersion(net::X509Certificate::OSCertHandle cert_handle);

net::CertType GetType(net::X509Certificate::OSCertHandle cert_handle);

std::string GetEmailAddress(net::X509Certificate::OSCertHandle cert_handle);

void GetUsageStrings(
    net::X509Certificate::OSCertHandle cert_handle,
    std::vector<std::string>* usages);

std::string GetKeyUsageString(net::X509Certificate::OSCertHandle cert_handle);

std::string GetSerialNumberHexified(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetIssuerCommonName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetIssuerOrgName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetIssuerOrgUnitName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetSubjectOrgName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetSubjectOrgUnitName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetSubjectCommonName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

bool GetTimes(net::X509Certificate::OSCertHandle cert_handle,
              base::Time* issued, base::Time* expires);

std::string GetTitle(net::X509Certificate::OSCertHandle cert_handle);
std::string GetIssuerName(net::X509Certificate::OSCertHandle cert_handle);
std::string GetSubjectName(net::X509Certificate::OSCertHandle cert_handle);

void GetEmailAddresses(net::X509Certificate::OSCertHandle cert_handle,
                       std::vector<std::string>* email_addresses);

void GetNicknameStringsFromCertList(const net::CertificateList& certs,
                                    const std::string& cert_expired,
                                    const std::string& cert_not_yet_valid,
                                    std::vector<std::string>* nick_names);

struct Extension {
  std::string name;
  std::string value;
};

typedef std::vector<Extension> Extensions;

void GetExtensions(
    const std::string& critical_label,
    const std::string& non_critical_label,
    net::X509Certificate::OSCertHandle cert_handle,
    Extensions* extensions);

// Hash a certificate using the given algorithm, return the result as a
// colon-seperated hex string.
std::string HashCertSHA256(net::X509Certificate::OSCertHandle cert_handle);
std::string HashCertSHA1(net::X509Certificate::OSCertHandle cert_handle);

// For host values, if they contain IDN Punycode-encoded A-labels, this will
// return a string suitable for display that contains both the original and the
// decoded U-label form.  Otherwise, the string will be returned as is.
std::string ProcessIDN(const std::string& input);

void GetCertChainFromCert(net::X509Certificate::OSCertHandle cert_handle,
                          net::X509Certificate::OSCertHandles* cert_handles);
void DestroyCertChain(net::X509Certificate::OSCertHandles* cert_handles);

std::string GetDerString(net::X509Certificate::OSCertHandle cert_handle);
std::string GetCMSString(const net::X509Certificate::OSCertHandles& cert_chain,
                         size_t start, size_t end);

std::string ProcessSecAlgorithmSignature(
    net::X509Certificate::OSCertHandle cert_handle);
std::string ProcessSecAlgorithmSubjectPublicKey(
    net::X509Certificate::OSCertHandle cert_handle);
std::string ProcessSecAlgorithmSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle);

std::string ProcessSubjectPublicKeyInfo(
    net::X509Certificate::OSCertHandle cert_handle);

std::string ProcessRawBitsSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle);

void RegisterDynamicOids();

}  // namespace x509_certificate_model

#endif  // NET_BASE_X509_CERTIFICATE_MODEL_H_
