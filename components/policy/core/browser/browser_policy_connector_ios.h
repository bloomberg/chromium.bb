// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_IOS_H_
#define COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_IOS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_export.h"

namespace policy {

class ProxyPolicyProvider;

// Extends BrowserPolicyConnector with the setup for iOS builds.
class POLICY_EXPORT BrowserPolicyConnectorIOS : public BrowserPolicyConnector {
 public:
  BrowserPolicyConnectorIOS(
      scoped_ptr<ConfigurationPolicyHandlerList> handler_list,
      const std::string& user_agent);

  virtual ~BrowserPolicyConnectorIOS();

  virtual void Init(
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context) OVERRIDE;

  // The browser-global PolicyService is created before Profiles are ready, to
  // provide managed values for the local state PrefService. It includes a
  // policy provider that forwards policies from a delegate policy provider.
  // This call can be used to set the user policy provider as that delegate
  // once the Profile is ready, so that user policies can also affect local
  // state preferences.
  // Only one user policy provider can be set as a delegate at a time, and any
  // previously set delegate is removed. Passing NULL removes the current
  // delegate, if there is one.
  void SetUserPolicyDelegate(ConfigurationPolicyProvider* user_policy_provider);

 private:
  std::string user_agent_;

  ProxyPolicyProvider* global_user_cloud_policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPolicyConnectorIOS);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_BROWSER_POLICY_CONNECTOR_IOS_H_
