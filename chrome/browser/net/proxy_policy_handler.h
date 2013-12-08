// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROXY_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_PROXY_POLICY_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class ProxyMap;
class ProxyErrorMap;

// ConfigurationPolicyHandler for the proxy policies.
class ProxyPolicyHandler : public ConfigurationPolicyHandler {
 public:
  // Constants for the "Proxy Server Mode" defined in the policies.
  // Note that these diverge from internal presentation defined in
  // ProxyPrefs::ProxyMode for legacy reasons. The following four
  // PolicyProxyModeType types were not very precise and had overlapping use
  // cases.
  enum ProxyModeType {
    // Disable Proxy, connect directly.
    PROXY_SERVER_MODE = 0,
    // Auto detect proxy or use specific PAC script if given.
    PROXY_AUTO_DETECT_PROXY_SERVER_MODE = 1,
    // Use manually configured proxy servers (fixed servers).
    PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE = 2,
    // Use system proxy server.
    PROXY_USE_SYSTEM_PROXY_SERVER_MODE = 3,

    MODE_COUNT
  };

  ProxyPolicyHandler();
  virtual ~ProxyPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const base::Value* GetProxyPolicyValue(const PolicyMap& policies,
                                         const char* policy_name);

  // Converts the deprecated ProxyServerMode policy value to a ProxyMode value
  // and places the result in |mode_value|. Returns whether the conversion
  // succeeded.
  bool CheckProxyModeAndServerMode(const PolicyMap& policies,
                                   PolicyErrorMap* errors,
                                   std::string* mode_value);

  DISALLOW_COPY_AND_ASSIGN(ProxyPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_PROXY_POLICY_HANDLER_H_
