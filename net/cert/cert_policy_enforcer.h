// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_CERT_CERT_POLICY_ENFORCER_H
#define NET_CERT_CERT_POLICY_ENFORCER_H

#include <stddef.h>

#include "net/base/net_export.h"

namespace net {

namespace ct {

struct CTVerifyResult;
class EVCertsWhitelist;

}  // namespace ct

class X509Certificate;

// Class for checking that a given certificate conforms to security-related
// policies.
class NET_EXPORT CertPolicyEnforcer {
 public:
  // Set the parameters for this policy enforcer:
  // |num_ct_logs| is the number of Certificate Transparency log currently
  // known to Chrome.
  // |require_ct_for_ev| indicates whether Certificate Transparency presence
  // is required for EV certificates.
  CertPolicyEnforcer(size_t num_ct_logs, bool require_ct_for_ev);
  virtual ~CertPolicyEnforcer();

  // Returns true if the collection of SCTs for the given certificate
  // conforms with the CT/EV policy.
  // |cert| is the certificate for which the SCTs apply.
  // |ct_result| must contain the result of verifying any SCTs associated with
  // |cert| prior to invoking this method.
  bool DoesConformToCTEVPolicy(X509Certificate* cert,
                               const ct::EVCertsWhitelist* ev_whitelist,
                               const ct::CTVerifyResult& ct_result);

 private:
  bool IsCertificateInWhitelist(X509Certificate* cert,
                                const ct::EVCertsWhitelist* ev_whitelist);

  bool HasRequiredNumberOfSCTs(X509Certificate* cert,
                               const ct::CTVerifyResult& ct_result);

  size_t num_ct_logs_;
  bool require_ct_for_ev_;
};

}  // namespace net

#endif  // NET_CERT_CERT_POLICY_ENFORCER_H
