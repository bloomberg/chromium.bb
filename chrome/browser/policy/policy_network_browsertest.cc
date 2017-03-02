// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

namespace {

// Checks if QUIC is enabled for new streams in the passed
// |request_context_getter|. Will set the bool pointed to by |quic_enabled|.
void IsQuicEnabledOnIOThread(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    bool* quic_enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  *quic_enabled = request_context_getter->GetURLRequestContext()
                      ->http_transaction_factory()
                      ->GetSession()
                      ->IsQuicEnabled();
}

// Can be called on the UI thread, returns if QUIC is enabled for new streams in
// the passed |request_context_getter|.
bool IsQuicEnabled(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop run_loop;
  bool is_quic_enabled = false;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(IsQuicEnabledOnIOThread, request_context_getter,
                 &is_quic_enabled),
      run_loop.QuitClosure());
  run_loop.Run();
  return is_quic_enabled;
}

// Short-hand access to global SafeBrowsingService's URLRequestContextGetter for
// better readability.
scoped_refptr<net::URLRequestContextGetter>
safe_browsing_service_request_context() {
  return g_browser_process->safe_browsing_service()->url_request_context();
}

// Short-hand access to global system request context getter for better
// readability.
scoped_refptr<net::URLRequestContextGetter> system_request_context() {
  return g_browser_process->system_request_context();
}

// Called when an additional profile has been created.
// The created profile is stored in *|out_created_profile|.
void OnProfileInitialized(Profile** out_created_profile,
                          const base::Closure& closure,
                          Profile* profile,
                          Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    *out_created_profile = profile;
    closure.Run();
  }
}

}  // namespace

namespace policy {

// The tests are based on the assumption that command line flag kEnableQuic
// guarantees that QUIC protocol is enabled which is the case at the moment
// when these are being written.
class QuicAllowedPolicyTestBase: public InProcessBrowserTest {
 public:
  QuicAllowedPolicyTestBase() : InProcessBrowserTest() {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    PolicyMap values;
    GetQuicAllowedPolicy(&values);
    provider_.UpdateChromePolicy(values);
  }

  virtual void GetQuicAllowedPolicy(PolicyMap* values) = 0;

 private:
  MockConfigurationPolicyProvider provider_;
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyTestBase);
};

// Policy QuicAllowed set to false.
class QuicAllowedPolicyIsFalse: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsFalse() : QuicAllowedPolicyTestBase() {}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(false),
                nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsFalse);
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsFalse, QuicDisallowed) {
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(browser()->profile()->GetRequestContext()));
}

// Policy QuicAllowed set to true.
class QuicAllowedPolicyIsTrue: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsTrue() : QuicAllowedPolicyTestBase() {}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
    values->Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(true),
                nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsTrue);
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsTrue, QuicAllowed) {
  EXPECT_TRUE(IsQuicEnabled(system_request_context()));
  EXPECT_TRUE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_TRUE(IsQuicEnabled(browser()->profile()->GetRequestContext()));
}

// Policy QuicAllowed is not set.
class QuicAllowedPolicyIsNotSet: public QuicAllowedPolicyTestBase {
 public:
  QuicAllowedPolicyIsNotSet() : QuicAllowedPolicyTestBase() {}

 protected:
  void GetQuicAllowedPolicy(PolicyMap* values) override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyIsNotSet);
};

IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyIsNotSet, NoQuicRegulations) {
  EXPECT_TRUE(IsQuicEnabled(system_request_context()));
  EXPECT_TRUE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_TRUE(IsQuicEnabled(browser()->profile()->GetRequestContext()));
}

// Policy QuicAllowed is set dynamically after profile creation.
// Supports creation of an additional profile.
class QuicAllowedPolicyDynamicTest : public InProcessBrowserTest {
 public:
  QuicAllowedPolicyDynamicTest()
      : InProcessBrowserTest(), profile_1_(nullptr), profile_2_(nullptr) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
    // Ensure that QUIC is enabled by default on browser startup.
    command_line->AppendSwitch(switches::kEnableQuic);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Set the overriden policy provider for the first Profile (profile_1_).
    EXPECT_CALL(policy_for_profile_1_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::ProfilePolicyConnectorFactory::GetInstance()
        ->PushProviderForTesting(&policy_for_profile_1_);
  }

  void SetUpOnMainThread() override { profile_1_ = browser()->profile(); }

  // Creates a second Profile for testing. The Profile can then be accessed by
  // profile_2() and its policy by policy_for_profile_2().
  void CreateSecondProfile() {
    EXPECT_FALSE(profile_2_);

    // Prepare policy provider for second profile.
    EXPECT_CALL(policy_for_profile_2_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::ProfilePolicyConnectorFactory::GetInstance()
        ->PushProviderForTesting(&policy_for_profile_2_);

    ProfileManager* profile_manager = g_browser_process->profile_manager();

    // Create an additional profile.
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path_profile,
        base::Bind(&OnProfileInitialized, &profile_2_, run_loop.QuitClosure()),
        base::string16(), std::string(), std::string());

    // Run the message loop to allow profile creation to take place; the loop is
    // terminated by OnProfileInitialized calling the loop's QuitClosure when
    // the profile is created.
    run_loop.Run();

    // Make sure second profile creation does what we think it does.
    EXPECT_TRUE(profile_1() != profile_2());
  }

  // Sets the QuicAllowed policy for a Profile.
  // |provider| is supposed to be the MockConfigurationPolicyProvider for the
  // Profile, as returned by policy_for_profile_1() / policy_for_profile_2().
  void SetQuicAllowedPolicy(MockConfigurationPolicyProvider* provider,
                            bool value) {
    PolicyMap policy_map;
    policy_map.Set(key::kQuicAllowed, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(value),
                   nullptr);
    provider->UpdateChromePolicy(policy_map);
    base::RunLoop().RunUntilIdle();
  }

  // Removes all policies for a Profile.
  // |provider| is supposed to be the MockConfigurationPolicyProvider for the
  // Profile, as returned by policy_for_profile_1() / policy_for_profile_2().
  void RemoveAllPolicies(MockConfigurationPolicyProvider* provider) {
    PolicyMap policy_map;
    provider->UpdateChromePolicy(policy_map);
    base::RunLoop().RunUntilIdle();
  }

  // Returns the first Profile.
  Profile* profile_1() { return profile_1_; }

  // Returns the second Profile. May only be called after CreateSecondProfile
  // has been called.
  Profile* profile_2() {
    // Only valid after CreateSecondProfile() has been called.
    EXPECT_TRUE(profile_2_);
    return profile_2_;
  }

  // Returns the MockConfigurationPolicyProvider for profile_1.
  MockConfigurationPolicyProvider* policy_for_profile_1() {
    return &policy_for_profile_1_;
  }

  // Returns the MockConfigurationPolicyProvider for profile_2.
  MockConfigurationPolicyProvider* policy_for_profile_2() {
    return &policy_for_profile_2_;
  }

 private:
  // The first profile.
  Profile* profile_1_;
  // The second profile. Only valid after CreateSecondProfile() has been called.
  Profile* profile_2_;

  // Mock Policy for profile_1_.
  MockConfigurationPolicyProvider policy_for_profile_1_;
  // Mock Policy for profile_2_.
  MockConfigurationPolicyProvider policy_for_profile_2_;

  DISALLOW_COPY_AND_ASSIGN(QuicAllowedPolicyDynamicTest);
};

// QUIC is disallowed by policy after the profile has been initialized.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest, QuicAllowedFalseThenTrue) {
  // After browser start, QuicAllowed=false comes in dynamically
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));

  // Set the QuicAllowed policy to true again
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));

  // Completely remove the QuicAllowed policy
  RemoveAllPolicies(policy_for_profile_1());
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));

  // QuicAllowed=false is set again
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));
}

// QUIC is allowed, then disallowed by policy after the profile has been
// initialized.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest, QuicAllowedTrueThenFalse) {
  // After browser start, QuicAllowed=true comes in dynamically
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  EXPECT_TRUE(IsQuicEnabled(system_request_context()));
  EXPECT_TRUE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()->GetRequestContext()));

  // Completely remove the QuicAllowed policy
  RemoveAllPolicies(policy_for_profile_1());
  EXPECT_TRUE(IsQuicEnabled(system_request_context()));
  EXPECT_TRUE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()->GetRequestContext()));

  // Set the QuicAllowed policy to true again
  SetQuicAllowedPolicy(policy_for_profile_1(), true);
  // Effectively, QUIC is still disabled because QUIC re-enabling is not
  // supported.
  EXPECT_TRUE(IsQuicEnabled(system_request_context()));
  EXPECT_TRUE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()->GetRequestContext()));

  // Now set QuicAllowed=false
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));
}

// A second Profile is created when QuicAllowed=false policy is in effect for
// the first profile.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       SecondProfileCreatedWhenQuicAllowedFalse) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));

  CreateSecondProfile();

  // QUIC is disabled in both profiles
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()->GetRequestContext()));
}

// A second Profile is created when no QuicAllowed policy is in effect for the
// first profile.
// Then QuicAllowed=false policy is dynamically set for both profiles.
IN_PROC_BROWSER_TEST_F(QuicAllowedPolicyDynamicTest,
                       QuicAllowedFalseAfterTwoProfilesCreated) {
  // If multiprofile mode is not enabled, you can't switch between profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  CreateSecondProfile();

  // QUIC is enabled in both profiles
  EXPECT_TRUE(IsQuicEnabled(system_request_context()));
  EXPECT_TRUE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_TRUE(IsQuicEnabled(profile_1()->GetRequestContext()));
  EXPECT_TRUE(IsQuicEnabled(profile_2()->GetRequestContext()));

  // Disable QUIC in first profile
  SetQuicAllowedPolicy(policy_for_profile_1(), false);
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));
  EXPECT_TRUE(IsQuicEnabled(profile_2()->GetRequestContext()));

  // Disable QUIC in second profile
  SetQuicAllowedPolicy(policy_for_profile_2(), false);
  EXPECT_FALSE(IsQuicEnabled(system_request_context()));
  EXPECT_FALSE(IsQuicEnabled(safe_browsing_service_request_context()));
  EXPECT_FALSE(IsQuicEnabled(profile_1()->GetRequestContext()));
  EXPECT_FALSE(IsQuicEnabled(profile_2()->GetRequestContext()));
}

}  // namespace policy
