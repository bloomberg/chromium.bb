// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"

#include "base/bind.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"

namespace {

// Testing factory that creates a FakeProfileOAuth2TokenService.
std::unique_ptr<KeyedService> BuildFakeProfileOAuth2TokenService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<FakeProfileOAuth2TokenService> service(
      new FakeProfileOAuth2TokenService(profile->GetPrefs()));
  return std::move(service);
}
}  // namespace

// static
std::unique_ptr<TestingProfile> IdentityTestEnvironmentProfileAdaptor::
    CreateProfileForIdentityTestEnvironment() {
  return CreateProfileForIdentityTestEnvironment(
      TestingProfile::TestingFactories());
}

// static
std::unique_ptr<TestingProfile>
IdentityTestEnvironmentProfileAdaptor::CreateProfileForIdentityTestEnvironment(
    const TestingProfile::TestingFactories& input_factories) {
  TestingProfile::Builder builder;

  for (auto& input_factory : input_factories) {
    builder.AddTestingFactory(input_factory.first, input_factory.second);
  }

  return CreateProfileForIdentityTestEnvironment(builder);
}

// static
std::unique_ptr<TestingProfile>
IdentityTestEnvironmentProfileAdaptor::CreateProfileForIdentityTestEnvironment(
    TestingProfile::Builder& builder) {
  for (auto& identity_factory : GetIdentityTestEnvironmentFactories()) {
    builder.AddTestingFactory(identity_factory.first, identity_factory.second);
  }

  return builder.Build();
}

// static
void IdentityTestEnvironmentProfileAdaptor::
    SetIdentityTestEnvironmentFactoriesOnBrowserContext(
        content::BrowserContext* context) {
  for (const auto& factory_pair : GetIdentityTestEnvironmentFactories()) {
    factory_pair.first->SetTestingFactory(context, factory_pair.second);
  }
}

// static
void IdentityTestEnvironmentProfileAdaptor::
    AppendIdentityTestEnvironmentFactories(
        TestingProfile::TestingFactories* factories_to_append_to) {
  TestingProfile::TestingFactories identity_factories =
      GetIdentityTestEnvironmentFactories();
  factories_to_append_to->insert(factories_to_append_to->end(),
                                 identity_factories.begin(),
                                 identity_factories.end());
}

// static
TestingProfile::TestingFactories
IdentityTestEnvironmentProfileAdaptor::GetIdentityTestEnvironmentFactories() {
  return {{ProfileOAuth2TokenServiceFactory::GetInstance(),
           base::BindRepeating(&BuildFakeProfileOAuth2TokenService)}};
}

// static
TestingProfile::TestingFactories IdentityTestEnvironmentProfileAdaptor::
    GetIdentityTestEnvironmentFactoriesWithPrimaryAccountSet(
        const std::string& email) {
  TestingProfile::TestingFactories testing_factories(
      GetIdentityTestEnvironmentFactories());
  testing_factories.emplace_back(
      IdentityManagerFactory::GetInstance(),
      base::BindRepeating(
          IdentityManagerFactory::BuildAuthenticatedServiceInstanceForTesting,
          identity::GetTestGaiaIdForEmail(email), email, "refresh_token"));

  return testing_factories;
}

IdentityTestEnvironmentProfileAdaptor::IdentityTestEnvironmentProfileAdaptor(
    Profile* profile)
    : identity_test_env_(
          profile->GetPrefs(),
          AccountTrackerServiceFactory::GetForProfile(profile),
          AccountFetcherServiceFactory::GetForProfile(profile),
          static_cast<FakeProfileOAuth2TokenService*>(
              ProfileOAuth2TokenServiceFactory::GetForProfile(profile)),
          SigninManagerFactory::GetForProfile(profile),
          GaiaCookieManagerServiceFactory::GetForProfile(profile),
          IdentityManagerFactory::GetForProfile(profile)) {}
