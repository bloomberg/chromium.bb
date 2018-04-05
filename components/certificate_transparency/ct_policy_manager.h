// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_POLICY_MANAGER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_POLICY_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/http/transport_security_state.h"

namespace certificate_transparency {

// CTPolicyManager serves as the bridge between the Certificate Transparency
// preferences (see pref_names.h) and the actual implementation, by exposing
// a TransportSecurityState::RequireCTDelegate that can be used to query for
// CT-related policies.
class CTPolicyManager {
 public:
  // Creates a CTPolicyManager that will provide a RequireCTDelegate delegate.
  CTPolicyManager();
  ~CTPolicyManager();

  // Returns a RequireCTDelegate that responds based on the policies set via
  // preferences.
  //
  // The order of priority of the preferences is that:
  //   - Specific hosts are preferred over those that match subdomains.
  //   - The most specific host is preferred.
  //   - Requiring CT is preferred over excluding CT
  //
  net::TransportSecurityState::RequireCTDelegate* GetDelegate();

  // Updates the CTDelegate to require CT for |required_hosts|, and exclude
  // |excluded_hosts| from CT policies.  In addtion, this method updates
  // |excluded_spkis| and |excluded_legacy_spkis| intended for use within an
  // Enterprise (see https://crbug.com/824184).
  void UpdateCTPolicies(const std::vector<std::string>& required_hosts,
                        const std::vector<std::string>& excluded_hosts,
                        const std::vector<std::string>& excluded_spkis,
                        const std::vector<std::string>& excluded_legacy_spkis);

 private:
  class CTDelegate;
  std::unique_ptr<CTDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CTPolicyManager);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_POLICY_MANAGER_H_
