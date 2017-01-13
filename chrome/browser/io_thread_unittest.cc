// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/mock_entropy_provider.h"
#include "build/build_config.h"
#include "chrome/browser/io_thread.h"
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
#include "net/http/http_network_session.h"
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
  static void ConfigureParamsFromFieldTrialsAndCommandLine(
      const base::CommandLine& command_line,
      bool is_quic_allowed_by_policy,
      net::HttpNetworkSession::Params* params) {
    IOThread::ConfigureParamsFromFieldTrialsAndCommandLine(
        command_line, is_quic_allowed_by_policy, false, params);
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
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD |
                       content::TestBrowserThreadBundle::DONT_CREATE_THREADS) {
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
    // The IOThread constructor registers the IOThread object with as the
    // BrowserThreadDelegate for the io thread.
    io_thread_.reset(new IOThread(&pref_service_, &policy_service_, nullptr,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                                  event_router_forwarder_.get()
#else
                                  nullptr
#endif
                                      ));
    // Now that IOThread object is registered starting the threads will
    // call the IOThread::Init(). This sets up the environment needed for
    // these tests.
    thread_bundle_.CreateThreads();
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
  TestingPrefServiceSimple pref_service_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<extensions::EventRouterForwarder> event_router_forwarder_;
#endif
  policy::PolicyMap policy_map_;
  policy::MockPolicyService policy_service_;
  // The ordering of the declarations of |io_thread_object_| and
  // |thread_bundle_| matters. An IOThread cannot be deleted until all of
  // the globals have been reset to their initial state via CleanUp. As
  // TestBrowserThreadBundle's destructor is responsible for calling
  // CleanUp(), the IOThread must be declared before the bundle, so that
  // the bundle is deleted first.
  std::unique_ptr<IOThread> io_thread_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(IOThreadTestWithIOThreadObject, UpdateNegotiateDisableCnameLookup) {
  // This test uses the kDisableAuthNegotiateCnameLookup to check that
  // the HttpAuthPreferences are correctly initialized and running on the
  // IO thread. The other preferences are tested by the HttpAuthPreferences
  // unit tests.
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

class ConfigureParamsFromFieldTrialsAndCommandLineTest
    : public ::testing::Test {
 public:
  ConfigureParamsFromFieldTrialsAndCommandLineTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        is_quic_allowed_by_policy_(true) {}

 protected:
  void ConfigureParamsFromFieldTrialsAndCommandLine() {
    IOThreadPeer::ConfigureParamsFromFieldTrialsAndCommandLine(
        command_line_, is_quic_allowed_by_policy_, &params_);
  }

  base::CommandLine command_line_;
  bool is_quic_allowed_by_policy_;
  net::HttpNetworkSession::Params params_;
};

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest, Default) {
  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.enable_http2);
  EXPECT_FALSE(params_.enable_quic);
  EXPECT_TRUE(params_.enable_quic_alternative_service_with_different_host);
  EXPECT_EQ(1350u, params_.quic_max_packet_length);
  EXPECT_EQ(net::QuicTagVector(), params_.quic_connection_options);
  EXPECT_TRUE(params_.origins_to_force_quic_on.empty());
  EXPECT_TRUE(params_.quic_host_whitelist.empty());
  EXPECT_FALSE(params_.enable_user_alternate_protocol_ports);
  EXPECT_FALSE(params_.ignore_certificate_errors);
  EXPECT_EQ(0, params_.testing_fixed_http_port);
  EXPECT_EQ(0, params_.testing_fixed_https_port);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       Http2CommandLineDisableHttp2) {
  command_line_.AppendSwitch("disable-http2");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_http2);
}

// Command line flag should not only disable QUIC but should also prevent QUIC
// related field trials and command line flags from being parsed.
TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       DisableQuicFromCommandLineOverridesFieldTrial) {
  auto field_trial_list =
      base::MakeUnique<base::FieldTrialList>(
          base::MakeUnique<base::MockEntropyProvider>());
  variations::testing::ClearAllVariationParams();

  std::map<std::string, std::string> field_trial_params;
  field_trial_params["always_require_handshake_confirmation"] = "true";
  field_trial_params["disable_delay_tcp_race"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  command_line_.AppendSwitch("disable-quic");
  command_line_.AppendSwitchASCII("quic-host-whitelist",
                                  "www.example.org, www.example.com");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_quic);
  EXPECT_FALSE(params_.quic_always_require_handshake_confirmation);
  EXPECT_TRUE(params_.quic_delay_tcp_race);
  EXPECT_TRUE(params_.quic_host_whitelist.empty());
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       EnableQuicFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       EnableAlternativeServicesFromCommandLineWithQuicDisabled) {
  command_line_.AppendSwitch("enable-alternative-services");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_quic);
  EXPECT_TRUE(params_.enable_quic_alternative_service_with_different_host);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       EnableAlternativeServicesFromCommandLineWithQuicEnabled) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitch("enable-alternative-services");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_TRUE(params_.enable_quic_alternative_service_with_different_host);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       QuicVersionFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  std::string version =
      net::QuicVersionToString(net::AllSupportedVersions().back());
  command_line_.AppendSwitchASCII("quic-version", version);

  ConfigureParamsFromFieldTrialsAndCommandLine();

  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::AllSupportedVersions().back());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       QuicConnectionOptionsFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-connection-options", "TIME,TBBR,REJ");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  net::QuicTagVector options;
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params_.quic_connection_options);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       QuicOriginsToForceQuicOn) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("origin-to-force-quic-on",
                                  "www.example.com:443, www.example.org:443");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_EQ(2u, params_.origins_to_force_quic_on.size());
  EXPECT_TRUE(
      base::ContainsKey(params_.origins_to_force_quic_on,
                        net::HostPortPair::FromString("www.example.com:443")));
  EXPECT_TRUE(
      base::ContainsKey(params_.origins_to_force_quic_on,
                        net::HostPortPair::FromString("www.example.org:443")));
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       QuicOriginsToForceQuicOnAll) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("origin-to-force-quic-on", "*");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_EQ(1u, params_.origins_to_force_quic_on.size());
  EXPECT_TRUE(
      base::ContainsKey(params_.origins_to_force_quic_on, net::HostPortPair()));
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       QuicWhitelistFromCommandLinet) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-host-whitelist",
                                  "www.example.org, www.example.com");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_EQ(2u, params_.quic_host_whitelist.size());
  EXPECT_TRUE(
      base::ContainsKey(params_.quic_host_whitelist, "www.example.org"));
  EXPECT_TRUE(
      base::ContainsKey(params_.quic_host_whitelist, "www.example.com"));
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       QuicDisallowedByPolicy) {
  command_line_.AppendSwitch("enable-quic");
  is_quic_allowed_by_policy_ = false;

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_quic);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest, QuicMaxPacketLength) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-max-packet-length", "1450");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_EQ(1450u, params_.quic_max_packet_length);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       EnableUserAlternateProtocolPorts) {
  command_line_.AppendSwitch("enable-user-controlled-alternate-protocol-ports");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.enable_user_alternate_protocol_ports);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest,
       IgnoreCertificateErrors) {
  command_line_.AppendSwitch("ignore-certificate-errors");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.ignore_certificate_errors);
}

TEST_F(ConfigureParamsFromFieldTrialsAndCommandLineTest, TestingFixedPort) {
  command_line_.AppendSwitchASCII("testing-fixed-http-port", "42");
  command_line_.AppendSwitchASCII("testing-fixed-https-port", "137");

  ConfigureParamsFromFieldTrialsAndCommandLine();

  EXPECT_EQ(42u, params_.testing_fixed_http_port);
  EXPECT_EQ(137u, params_.testing_fixed_https_port);
}

}  // namespace test
