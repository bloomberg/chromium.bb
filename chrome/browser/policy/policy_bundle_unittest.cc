// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_bundle.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kPolicyClashing0[] = "policy-clashing-0";
const char kPolicyClashing1[] = "policy-clashing-1";
const char kPolicy0[] = "policy-0";
const char kPolicy1[] = "policy-1";
const char kPolicy2[] = "policy-2";
const char kExtension0[] = "extension-0";
const char kExtension1[] = "extension-1";
const char kExtension2[] = "extension-2";
const char kExtension3[] = "extension-3";

// Adds test policies to |policy|.
void AddTestPolicies(PolicyMap* policy) {
  policy->Set("mandatory-user", POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, base::Value::CreateIntegerValue(123));
  policy->Set("mandatory-machine", POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_MACHINE, base::Value::CreateStringValue("omg"));
  policy->Set("recommended-user", POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetBoolean("false", false);
  dict->SetInteger("int", 456);
  dict->SetString("str", "bbq");
  policy->Set("recommended-machine", POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_MACHINE, dict);
}

// Adds test policies to |policy| based on the parameters:
// - kPolicyClashing0 mapped to |value|, user mandatory
// - kPolicyClashing1 mapped to |value|, with |level| and |scope|
// - |name| mapped to |value|, user mandatory
void AddTestPoliciesWithParams(PolicyMap *policy,
                               const char* name,
                               int value,
                               PolicyLevel level,
                               PolicyScope scope) {
  policy->Set(kPolicyClashing0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              base::Value::CreateIntegerValue(value));
  policy->Set(kPolicyClashing1, level, scope,
              base::Value::CreateIntegerValue(value));
  policy->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              base::Value::CreateIntegerValue(value));
}

// Returns true if |bundle| is empty.
bool IsEmpty(const PolicyBundle& bundle) {
  return bundle.begin() == bundle.end();
}

}  // namespace

TEST(PolicyBundleTest, Get) {
  PolicyBundle bundle;
  EXPECT_TRUE(IsEmpty(bundle));

  AddTestPolicies(&bundle.Get(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_FALSE(IsEmpty(bundle));

  PolicyMap policy;
  AddTestPolicies(&policy);
  EXPECT_TRUE(bundle.Get(POLICY_DOMAIN_CHROME, std::string()).Equals(policy));

  PolicyBundle::const_iterator it = bundle.begin();
  ASSERT_NE(it, bundle.end());
  EXPECT_EQ(POLICY_DOMAIN_CHROME, it->first.first);
  EXPECT_EQ("", it->first.second);
  ASSERT_TRUE(it->second);
  EXPECT_TRUE(it->second->Equals(policy));
  ++it;
  EXPECT_EQ(bundle.end(), it);
  EXPECT_TRUE(bundle.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).empty());

  EXPECT_FALSE(IsEmpty(bundle));
  bundle.Clear();
  EXPECT_TRUE(IsEmpty(bundle));
}

TEST(PolicyBundleTest, SwapAndCopy) {
  PolicyBundle bundle0;
  PolicyBundle bundle1;

  AddTestPolicies(&bundle0.Get(POLICY_DOMAIN_CHROME, std::string()));
  AddTestPolicies(&bundle0.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0));
  EXPECT_FALSE(IsEmpty(bundle0));
  EXPECT_TRUE(IsEmpty(bundle1));

  PolicyMap policy;
  AddTestPolicies(&policy);
  EXPECT_TRUE(bundle0.Get(POLICY_DOMAIN_CHROME, std::string()).Equals(policy));
  EXPECT_TRUE(
      bundle0.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).Equals(policy));

  bundle0.Swap(&bundle1);
  EXPECT_TRUE(IsEmpty(bundle0));
  EXPECT_FALSE(IsEmpty(bundle1));

  EXPECT_TRUE(bundle1.Get(POLICY_DOMAIN_CHROME, std::string()).Equals(policy));
  EXPECT_TRUE(
      bundle1.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).Equals(policy));

  bundle0.CopyFrom(bundle1);
  EXPECT_FALSE(IsEmpty(bundle0));
  EXPECT_FALSE(IsEmpty(bundle1));
  EXPECT_TRUE(bundle0.Get(POLICY_DOMAIN_CHROME, std::string()).Equals(policy));
  EXPECT_TRUE(
      bundle0.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).Equals(policy));
}

TEST(PolicyBundleTest, MergeFrom) {
  // Each bundleN has kExtensionN. Each bundle also has policy for
  // chrome and kExtension3.
  // |bundle0| has the highest priority, |bundle2| the lowest.
  PolicyBundle bundle0;
  PolicyBundle bundle1;
  PolicyBundle bundle2;

  PolicyMap policy0;
  AddTestPoliciesWithParams(
      &policy0, kPolicy0, 0u, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER);
  bundle0.Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(policy0);
  bundle0.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).CopyFrom(policy0);
  bundle0.Get(POLICY_DOMAIN_EXTENSIONS, kExtension3).CopyFrom(policy0);

  PolicyMap policy1;
  AddTestPoliciesWithParams(
      &policy1, kPolicy1, 1u, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE);
  bundle1.Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(policy1);
  bundle1.Get(POLICY_DOMAIN_EXTENSIONS, kExtension1).CopyFrom(policy1);
  bundle1.Get(POLICY_DOMAIN_EXTENSIONS, kExtension3).CopyFrom(policy1);

  PolicyMap policy2;
  AddTestPoliciesWithParams(
      &policy2, kPolicy2, 2u, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  bundle2.Get(POLICY_DOMAIN_CHROME, std::string()).CopyFrom(policy2);
  bundle2.Get(POLICY_DOMAIN_EXTENSIONS, kExtension2).CopyFrom(policy2);
  bundle2.Get(POLICY_DOMAIN_EXTENSIONS, kExtension3).CopyFrom(policy2);

  // Merge in order of decreasing priority.
  PolicyBundle merged;
  merged.MergeFrom(bundle0);
  merged.MergeFrom(bundle1);
  merged.MergeFrom(bundle2);
  PolicyBundle empty_bundle;
  merged.MergeFrom(empty_bundle);

  // chrome and kExtension3 policies are merged:
  // - kPolicyClashing0 comes from bundle0, which has the highest priority;
  // - kPolicyClashing1 comes from bundle1, which has the highest level/scope
  //   combination;
  // - kPolicyN are merged from each bundle.
  PolicyMap expected;
  expected.Set(kPolicyClashing0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(0));
  expected.Set(kPolicyClashing1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               base::Value::CreateIntegerValue(1));
  expected.Set(kPolicy0, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(0));
  expected.Set(kPolicy1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(1));
  expected.Set(kPolicy2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(2));
  EXPECT_TRUE(merged.Get(POLICY_DOMAIN_CHROME, std::string()).Equals(expected));
  EXPECT_TRUE(
      merged.Get(POLICY_DOMAIN_EXTENSIONS, kExtension3).Equals(expected));
  // extension0 comes only from bundle0.
  EXPECT_TRUE(
      merged.Get(POLICY_DOMAIN_EXTENSIONS, kExtension0).Equals(policy0));
  // extension1 comes only from bundle1.
  EXPECT_TRUE(
      merged.Get(POLICY_DOMAIN_EXTENSIONS, kExtension1).Equals(policy1));
  // extension2 comes only from bundle2.
  EXPECT_TRUE(
      merged.Get(POLICY_DOMAIN_EXTENSIONS, kExtension2).Equals(policy2));
}

}  // namespace policy
