// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/cert/x509_certificate.h"

namespace net {
class CertTrustAnchorProvider;
}

namespace policy {

// Keeps track of the network configuration policy settings and pushes changes
// to the respective configuration backend, which in turn writes configurations
// to Shill.
class NetworkConfigurationUpdater {
 public:
  NetworkConfigurationUpdater();
  virtual ~NetworkConfigurationUpdater();

  // Notifies this updater that the user policy is initialized. Before this
  // function is called, the user policy is not applied. This function may
  // trigger immediate policy applications.
  // Web trust isn't given to certificates imported from ONC by default. Setting
  // |allow_trust_certs_from_policy| to true allows giving Web trust to the
  // certificates that request it.
  virtual void OnUserPolicyInitialized(bool allow_trusted_certs_from_policy,
                                       const std::string& hashed_username) = 0;

  // Returns a CertTrustAnchorProvider that provides the list of server and
  // CA certificates with the Web trust flag set that were retrieved from the
  // last user ONC policy update.
  // This getter must be used on the UI thread, and the provider must be used
  // on the IO thread. It is only valid as long as the
  // NetworkConfigurationUpdater is valid; the NetworkConfigurationUpdater
  // outlives all the profiles, and deletes the provider on the IO thread.
  net::CertTrustAnchorProvider* GetCertTrustAnchorProvider();

 protected:
  void SetAllowTrustedCertsFromPolicy();

  void SetTrustAnchors(scoped_ptr<net::CertificateList> web_trust_certs);

 private:
  // Whether Web trust is allowed or not.
  bool allow_trusted_certificates_from_policy_;

  // An implementation of CertTrustAnchorProvider. Owned by this class, but
  // runs and is deleted on the IO thread.
  net::CertTrustAnchorProvider* cert_trust_provider_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
