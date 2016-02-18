// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_POLICY_STATUS_H
#define NET_CERT_CT_POLICY_STATUS_H

namespace net {

namespace ct {

// Information about a connection's compliance with the CT EV
// certificate policy.
enum class EVPolicyCompliance {
  // The certificate was not EV, so the EV policy doesn't apply.
  EV_POLICY_DOES_NOT_APPLY,
  // The connection complied with the EV certificate policy by being
  // included on the EV whitelist.
  EV_POLICY_COMPLIES_VIA_WHITELIST,
  // The connection complied with the EV certificate policy by
  // including SCTs that satisfy the policy.
  EV_POLICY_COMPLIES_VIA_SCTS,
  // The connection did not have enough SCTs to retain its EV
  // status.
  EV_POLICY_NOT_ENOUGH_SCTS,
  // The connection did not have diverse enough SCTs to retain its
  // EV status.
  EV_POLICY_NOT_DIVERSE_SCTS,
  // The connection cannot be considered compliant because the build
  // isn't timely and therefore log information might be out of date
  // (for example a log might no longer be considered trustworthy).
  EV_POLICY_BUILD_NOT_TIMELY,
};

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_POLICY_STATUS_H
