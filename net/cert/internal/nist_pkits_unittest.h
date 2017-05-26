// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_NIST_PKITS_UNITTEST_H_
#define NET_CERT_INTERNAL_NIST_PKITS_UNITTEST_H_

#include <set>

#include "net/cert/internal/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Describes additional inputs to verification in the PKITS tests
// (which are referred to as "settings" in that document).
struct PkitsTestSettings {
  // Default construction results in the "default settings".
  PkitsTestSettings();
  ~PkitsTestSettings();

  // Sets |initial_policy_set| to the specified policies. The
  // policies are described as comma-separated symbolic strings like
  // "anyPolicy" and "NIST-test-policy-1".
  void SetInitialPolicySet(const char* const policy_names);

  // A set of policy OIDs to use for "initial-policy-set".
  std::set<der::Input> initial_policy_set;

  // The value of "initial-explicit-policy".
  bool initial_explicit_policy = false;

  // The value of "initial-policy-mapping-inhibit".
  bool initial_policy_mapping_inhibit = false;

  // The value of "initial-inhibit-any-policy".
  bool initial_inhibit_any_policy = false;
};

// Parameterized test class for PKITS tests.
// The instantiating code should define a PkitsTestDelegate with an appropriate
// static Verify method, and then INSTANTIATE_TYPED_TEST_CASE_P for each
// testcase (each TYPED_TEST_CASE_P in pkits_testcases-inl.h).
template <typename PkitsTestDelegate>
class PkitsTest : public ::testing::Test {
 public:
  template <size_t num_certs, size_t num_crls>
  bool Verify(const char* const (&cert_names)[num_certs],
              const char* const (&crl_names)[num_crls],
              const PkitsTestSettings& settings) {
    std::vector<std::string> cert_ders;
    for (const std::string& s : cert_names)
      cert_ders.push_back(net::ReadTestFileToString(
          "net/third_party/nist-pkits/certs/" + s + ".crt"));
    std::vector<std::string> crl_ders;
    for (const std::string& s : crl_names)
      crl_ders.push_back(net::ReadTestFileToString(
          "net/third_party/nist-pkits/crls/" + s + ".crl"));
    return PkitsTestDelegate::Verify(cert_ders, crl_ders, settings);
  }
};

// Inline the generated test code:
#include "net/third_party/nist-pkits/pkits_testcases-inl.h"

}  // namespace net

#endif  // NET_CERT_INTERNAL_NIST_PKITS_UNITTEST_H_
