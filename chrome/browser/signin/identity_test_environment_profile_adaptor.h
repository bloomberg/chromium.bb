// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_PROFILE_ADAPTOR_H_
#define CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_PROFILE_ADAPTOR_H_

#include "chrome/test/base/testing_profile.h"
#include "services/identity/public/cpp/identity_test_environment.h"

// Adaptor that supports identity::IdentityTestEnvironment's usage in testing
// contexts where the relevant fake objects must be injected via the
// BrowserContextKeyedServiceFactory infrastructure as the production code
// accesses IdentityManager via that infrastructure. Before using this
// class, please consider whether you can change the production code in question
// to take in the relevant dependencies directly rather than obtaining them from
// the Profile; this is both cleaner in general and allows for direct usage of
// identity::IdentityTestEnvironment in the test.
class IdentityTestEnvironmentProfileAdaptor {
 public:
  // This static method must be called as part of configuring the TestingProfile
  // that the test is using. It will append the set of
  // testing factories that identity::IdentityTestEnvironment
  // requires to |factories_to_append_to|, which should be the set of testing
  // factories supplied to TestingProfile (via one of the various mechanisms for
  // doing so, e.g. using a TestingProfile::Builder).
  static void AppendIdentityTestEnvironmentFactories(
      TestingProfile::TestingFactories* factories_to_append_to);

  // Constructs an adaptor that associates an IdentityTestEnvironment instance
  // with |profile| via the relevant backing objects. Note that
  // AppendIdentityTestEnvironmentFactories() must have previously been invoked
  // on the set of testing factories used to configure |profile|.
  // |profile| must outlive this object.
  explicit IdentityTestEnvironmentProfileAdaptor(Profile* profile);
  ~IdentityTestEnvironmentProfileAdaptor() {}

  // Returns the IdentityTestEnvironment associated with this object (and
  // implicitly with the Profile passed to this object's constructor).
  identity::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  identity::IdentityTestEnvironment identity_test_env_;

  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironmentProfileAdaptor);
};

#endif  // CHROME_BROWSER_SIGNIN_IDENTITY_TEST_ENVIRONMENT_PROFILE_ADAPTOR_H_
