// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/mock_entropy_provider.h"
#include "build/build_config.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/features/features.h"
#include "net/cert_net/nss_ocsp.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/quic/chromium/quic_stream_factory.h"
#include "net/quic/core/quic_tag.h"
#include "net/quic/core/quic_versions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#endif

namespace test {

using ::testing::ReturnRef;

// Class used for accessing IOThread methods (friend of IOThread).
class IOThreadPeer {
 public:
  static net::HttpAuthPreferences* GetAuthPreferences(IOThread* io_thread) {
    return io_thread->globals()->http_auth_preferences.get();
  }
};

class IOThreadTestWithIOThreadObject : public testing::Test {
 public:
  // These functions need to be public, since it is difficult to bind to
  // protected functions in a test (the code would need to explicitly contain
  // the name of the actual test class).
  void CheckCnameLookup(bool expected) {
    auto* http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    ASSERT_NE(nullptr, http_auth_preferences);
    EXPECT_EQ(expected, http_auth_preferences->NegotiateDisableCnameLookup());
  }

  void CheckNegotiateEnablePort(bool expected) {
    auto* http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    ASSERT_NE(nullptr, http_auth_preferences);
    EXPECT_EQ(expected, http_auth_preferences->NegotiateEnablePort());
  }

  void CheckEffectiveConnectionType(net::EffectiveConnectionType expected) {
    ASSERT_EQ(expected,
              io_thread_->globals()
                  ->network_quality_estimator->GetEffectiveConnectionType());
  }

#if defined(OS_ANDROID)
  void CheckAuthAndroidNegoitateAccountType(std::string expected) {
    auto* http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    ASSERT_NE(nullptr, http_auth_preferences);
    EXPECT_EQ(expected,
              http_auth_preferences->AuthAndroidNegotiateAccountType());
  }
#endif

  void CheckCanUseDefaultCredentials(bool expected, const GURL& url) {
    auto* http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    EXPECT_EQ(expected, http_auth_preferences->CanUseDefaultCredentials(url));
  }

  void CheckCanDelegate(bool expected, const GURL& url) {
    auto* http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    EXPECT_EQ(expected, http_auth_preferences->CanDelegate(url));
  }

 protected:
  IOThreadTestWithIOThreadObject()
      : thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD |
            content::TestBrowserThreadBundle::DONT_CREATE_BROWSER_THREADS) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    event_router_forwarder_ = new extensions::EventRouterForwarder;
#endif
    PrefRegistrySimple* pref_registry = pref_service_.registry();
    IOThread::RegisterPrefs(pref_registry);
    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_registry);
    ssl_config::SSLConfigServiceManager::RegisterPrefs(pref_registry);

    // Set up default function behaviour.
    EXPECT_CALL(policy_service_,
                GetPolicies(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));

#if defined(OS_CHROMEOS)
    // Needed by IOThread constructor.
    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkHandler::Initialize();
#endif
  }

  void CreateThreads() {
    // The IOThread constructor registers the IOThread object with as the
    // BrowserThreadDelegate for the io thread.
    io_thread_.reset(new IOThread(&pref_service_, &policy_service_, nullptr,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                                  event_router_forwarder_.get(),
#else
                                  nullptr,
#endif
                                  &system_network_context_manager_));
    // Now that IOThread object is registered starting the threads will
    // call the IOThread::Init(). This sets up the environment needed for
    // these tests.
    thread_bundle_.CreateBrowserThreads();
  }

  ~IOThreadTestWithIOThreadObject() override {
#if defined(USE_NSS_CERTS)
    // Reset OCSPIOLoop thread checks, so that the test runner can run
    // futher tests in the same process.
    RunOnIOThreadBlocking(base::Bind(&net::ResetNSSHttpIOForTesting));
#endif
#if defined(OS_CHROMEOS)
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
#endif
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  void RunOnIOThreadBlocking(const base::Closure& task) {
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE, task, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::ShadowingAtExitManager at_exit_manager_;

  // |pref_service_| must outlive |io_thread_|.
  TestingPrefServiceSimple pref_service_;

  // The ordering of the declarations of |io_thread_object_| and
  // |thread_bundle_| matters. An IOThread cannot be deleted until all of
  // the globals have been reset to their initial state via CleanUp. As
  // TestBrowserThreadBundle's destructor is responsible for calling
  // CleanUp(), the IOThread must be declared before the bundle, so that
  // the bundle is deleted first.
  std::unique_ptr<IOThread> io_thread_;
  content::TestBrowserThreadBundle thread_bundle_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> event_router_forwarder_;
#endif
  policy::PolicyMap policy_map_;
  // |policy_service_| must be instantiated after |thread_bundle_|.
  policy::MockPolicyService policy_service_;
  SystemNetworkContextManager system_network_context_manager_;
};

TEST_F(IOThreadTestWithIOThreadObject, UpdateNegotiateDisableCnameLookup) {
  // This test uses the kDisableAuthNegotiateCnameLookup to check that
  // the HttpAuthPreferences are correctly initialized and running on the
  // IO thread. The other preferences are tested by the HttpAuthPreferences
  // unit tests.
  CreateThreads();
  pref_service()->SetBoolean(prefs::kDisableAuthNegotiateCnameLookup, false);
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckCnameLookup,
                 base::Unretained(this), false));
  pref_service()->SetBoolean(prefs::kDisableAuthNegotiateCnameLookup, true);
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckCnameLookup,
                 base::Unretained(this), true));
}

TEST_F(IOThreadTestWithIOThreadObject, UpdateEnableAuthNegotiatePort) {
  CreateThreads();
  pref_service()->SetBoolean(prefs::kEnableAuthNegotiatePort, false);
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckNegotiateEnablePort,
                 base::Unretained(this), false));
  pref_service()->SetBoolean(prefs::kEnableAuthNegotiatePort, true);
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckNegotiateEnablePort,
                 base::Unretained(this), true));
}

TEST_F(IOThreadTestWithIOThreadObject, UpdateServerWhitelist) {
  CreateThreads();
  GURL url("http://test.example.com");

  pref_service()->SetString(prefs::kAuthServerWhitelist, "xxx");
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckCanUseDefaultCredentials,
                 base::Unretained(this), false, url));

  pref_service()->SetString(prefs::kAuthServerWhitelist, "*");
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckCanUseDefaultCredentials,
                 base::Unretained(this), true, url));
}

TEST_F(IOThreadTestWithIOThreadObject, UpdateDelegateWhitelist) {
  CreateThreads();
  GURL url("http://test.example.com");

  pref_service()->SetString(prefs::kAuthNegotiateDelegateWhitelist, "");
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckCanDelegate,
                 base::Unretained(this), false, url));

  pref_service()->SetString(prefs::kAuthNegotiateDelegateWhitelist, "*");
  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckCanDelegate,
                 base::Unretained(this), true, url));
}

#if defined(OS_ANDROID)
// AuthAndroidNegotiateAccountType is only used on Android.
TEST_F(IOThreadTestWithIOThreadObject, UpdateAuthAndroidNegotiateAccountType) {
  CreateThreads();
  pref_service()->SetString(prefs::kAuthAndroidNegotiateAccountType, "acc1");
  RunOnIOThreadBlocking(base::Bind(
      &IOThreadTestWithIOThreadObject::CheckAuthAndroidNegoitateAccountType,
      base::Unretained(this), "acc1"));
  pref_service()->SetString(prefs::kAuthAndroidNegotiateAccountType, "acc2");
  RunOnIOThreadBlocking(base::Bind(
      &IOThreadTestWithIOThreadObject::CheckAuthAndroidNegoitateAccountType,
      base::Unretained(this), "acc2"));
}
#endif

TEST_F(IOThreadTestWithIOThreadObject, ForceECTFromCommandLine) {
  base::CommandLine::Init(0, nullptr);
  ASSERT_TRUE(base::CommandLine::InitializedForCurrentProcess());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--force-effective-connection-type", "Slow-2G");

  // Create threads after initializing the command line.
  CreateThreads();

  RunOnIOThreadBlocking(base::Bind(
      &IOThreadTestWithIOThreadObject::CheckEffectiveConnectionType,
      base::Unretained(this), net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G));
}

TEST_F(IOThreadTestWithIOThreadObject, ForceECTUsingFieldTrial) {
  base::CommandLine::Init(0, nullptr);
  ASSERT_TRUE(base::CommandLine::InitializedForCurrentProcess());

  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;
  variation_params["force_effective_connection_type"] = "2G";
  ASSERT_TRUE(variations::AssociateVariationParams(
      "NetworkQualityEstimator", "Enabled", variation_params));

  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("NetworkQualityEstimator",
                                                     "Enabled"));

  // Create threads after the field trial has been set.
  CreateThreads();

  RunOnIOThreadBlocking(
      base::Bind(&IOThreadTestWithIOThreadObject::CheckEffectiveConnectionType,
                 base::Unretained(this), net::EFFECTIVE_CONNECTION_TYPE_2G));
}

TEST_F(IOThreadTestWithIOThreadObject, ECTFromCommandLineOverridesFieldTrial) {
  base::CommandLine::Init(0, nullptr);
  ASSERT_TRUE(base::CommandLine::InitializedForCurrentProcess());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--force-effective-connection-type", "Slow-2G");

  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;
  variation_params["force_effective_connection_type"] = "2G";
  ASSERT_TRUE(variations::AssociateVariationParams(
      "NetworkQualityEstimator", "Enabled", variation_params));

  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("NetworkQualityEstimator",
                                                     "Enabled"));

  // Create threads after the field trial has been set.
  CreateThreads();
  RunOnIOThreadBlocking(base::Bind(
      &IOThreadTestWithIOThreadObject::CheckEffectiveConnectionType,
      base::Unretained(this), net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G));
}

}  // namespace test
