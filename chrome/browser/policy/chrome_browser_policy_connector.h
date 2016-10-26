// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
#define CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/browser/browser_policy_connector.h"

class PrefService;

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class ConfigurationPolicyProvider;

// Extends BrowserPolicyConnector with the setup shared among the desktop
// implementations and Android.
class ChromeBrowserPolicyConnector : public BrowserPolicyConnector {
 public:
  // Service initialization delay time in millisecond on startup. (So that
  // displaying Chrome's GUI does not get delayed.)
  static const int64_t kServiceInitializationStartupDelay = 5000;

  // Builds an uninitialized ChromeBrowserPolicyConnector, suitable for testing.
  // Init() should be called to create and start the policy machinery.
  ChromeBrowserPolicyConnector();

  ~ChromeBrowserPolicyConnector() override;

  void Init(
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> request_context) override;

 private:
  std::unique_ptr<ConfigurationPolicyProvider> CreatePlatformProvider();

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserPolicyConnector);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CHROME_BROWSER_POLICY_CONNECTOR_H_
