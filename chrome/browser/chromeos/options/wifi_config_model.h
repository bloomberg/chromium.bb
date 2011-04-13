// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_MODEL_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "net/base/cert_database.h"

namespace chromeos {

// Data model for Wi-Fi connection configuration dialog.  Mostly concerned
// with certificate management for Extensible Authentication Protocol (EAP)
// enterprise networks.
class WifiConfigModel {
 public:
  // Constructs a model with empty lists of certificates.  If you are
  // configuring a 802.1X network, call UpdateCertificates() to build the
  // internal cache of certificate names and IDs.
  WifiConfigModel();
  ~WifiConfigModel();

  // Updates the cached certificate lists.
  void UpdateCertificates();

  // Returns the number of user certificates.
  int GetUserCertCount() const;

  // Returns a user-visible name for a given user certificate.
  string16 GetUserCertName(int cert_index) const;

  // Returns the PKCS#11 ID for a given user certificate.
  std::string GetUserCertPkcs11Id(int cert_index) const;

  // Returns the cert_index for a given PKCS#11 user certificate ID,
  // or -1 if no certificate with that ID exists.
  int GetUserCertIndex(const std::string& pkcs11_id) const;

  // Returns the number of server CA certificates.
  int GetServerCaCertCount() const;

  // Returns a user-visible name for a given server CA certificate.
  string16 GetServerCaCertName(int cert_index) const;

  // Returns the NSS nickname for a given server CA certificate.
  std::string GetServerCaCertNssNickname(int cert_index) const;

  // Returns the cert_index for a given server CA certificate NSS nickname,
  // or -1 if no certificate with that ID exists.
  int GetServerCaCertIndex(const std::string& nss_nickname) const;

 private:
  net::CertDatabase cert_db_;

  // List of user certificates, sorted by name.
  net::CertificateList user_certs_;

  // List of server CA certificates, sorted by name.
  net::CertificateList server_ca_certs_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigModel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OPTIONS_WIFI_CONFIG_MODEL_H_
