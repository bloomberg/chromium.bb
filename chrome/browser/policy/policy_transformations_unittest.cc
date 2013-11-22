// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_transformations.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(PolicyTransformationsTest, FixDeprecatedPolicies) {
  PolicyBundle policy_bundle;
  PolicyMap& policy_map =
      policy_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  // Individual proxy policy values in the Chrome namespace should be collected
  // into a dictionary.
  policy_map.Set(key::kProxyServerMode,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 base::Value::CreateIntegerValue(3),
                 NULL);

  // Both these policies should be ignored, since there's a higher priority
  // policy available.
  policy_map.Set(key::kProxyMode,
                 POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("pac_script"),
                 NULL);
  policy_map.Set(key::kProxyPacUrl,
                 POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER,
                 base::Value::CreateStringValue("http://example.com/wpad.dat"),
                 NULL);

  // Add a value to a non-Chrome namespace.
  policy_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, std::string()))
      .Set(key::kProxyServerMode,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           base::Value::CreateIntegerValue(3),
           NULL);

  PolicyBundle actual_bundle;
  actual_bundle.CopyFrom(policy_bundle);
  FixDeprecatedPolicies(&actual_bundle);

  PolicyBundle expected_bundle;
  // The resulting Chrome namespace map should have the collected policy.
  PolicyMap& expected_map =
      expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  scoped_ptr<base::DictionaryValue> expected_value(new base::DictionaryValue);
  expected_value->SetInteger(key::kProxyServerMode, 3);
  expected_map.Set(key::kProxySettings,
                   POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER,
                   expected_value.release(),
                   NULL);
  // The resulting Extensions namespace map shouldn't have been modified.
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, std::string()))
      .Set(key::kProxyServerMode,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER,
           base::Value::CreateIntegerValue(3),
           NULL);

  EXPECT_TRUE(expected_bundle.Equals(actual_bundle));
}

}  // namespace policy
