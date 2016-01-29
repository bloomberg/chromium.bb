// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/test/mock_entropy_provider.h"
#include "build/build_config.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert_net/nss_ocsp.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#endif

namespace test {

using ::testing::ElementsAre;
using ::testing::ReturnRef;

// Class used for accessing IOThread methods (friend of IOThread).
class IOThreadPeer {
 public:
  static void ConfigureQuicGlobals(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const std::map<std::string, std::string>& quic_trial_params,
      bool is_quic_allowed_by_policy,
      IOThread::Globals* globals) {
    IOThread::ConfigureQuicGlobals(command_line, quic_trial_group,
                                   quic_trial_params, is_quic_allowed_by_policy,
                                   globals);
  }

  static void ConfigureSpdyGlobals(
      const base::CommandLine& command_line,
      base::StringPiece spdy_trial_group,
      const std::map<std::string, std::string>& spdy_trial_params,
      IOThread::Globals* globals) {
    IOThread::ConfigureSpdyGlobals(command_line, spdy_trial_group,
                                   spdy_trial_params, globals);
  }

  static void ConfigureAltSvcGlobals(const base::CommandLine& command_line,
                                     base::StringPiece altsvc_trial_group,
                                     IOThread::Globals* globals) {
    IOThread::ConfigureAltSvcGlobals(command_line, altsvc_trial_group, globals);
  }

  static void ConfigureNPNGlobals(base::StringPiece npn_trial_group,
                                  IOThread::Globals* globals) {
    IOThread::ConfigureNPNGlobals(npn_trial_group, globals);
  }

  static void InitializeNetworkSessionParamsFromGlobals(
      const IOThread::Globals& globals,
      net::HttpNetworkSession::Params* params) {
    IOThread::InitializeNetworkSessionParamsFromGlobals(globals, params);
  }

  static net::HttpAuthPreferences* GetAuthPreferences(IOThread* io_thread) {
    return io_thread->globals()->http_auth_preferences.get();
  }
};

class IOThreadTest : public testing::Test {
 public:
  IOThreadTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        is_quic_allowed_by_policy_(true) {
    globals_.http_server_properties.reset(new net::HttpServerPropertiesImpl());
  }

  void ConfigureQuicGlobals() {
    IOThreadPeer::ConfigureQuicGlobals(command_line_,
                                       field_trial_group_,
                                       field_trial_params_,
                                       is_quic_allowed_by_policy_,
                                       &globals_);
  }

  void ConfigureSpdyGlobals() {
    IOThreadPeer::ConfigureSpdyGlobals(command_line_, field_trial_group_,
                                       field_trial_params_, &globals_);
  }

  void ConfigureAltSvcGlobals() {
    IOThreadPeer::ConfigureAltSvcGlobals(command_line_, field_trial_group_,
                                         &globals_);
  }

  void ConfigureNPNGlobals() {
    IOThreadPeer::ConfigureNPNGlobals(field_trial_group_, &globals_);
  }

  void InitializeNetworkSessionParams(net::HttpNetworkSession::Params* params) {
    IOThreadPeer::InitializeNetworkSessionParamsFromGlobals(globals_, params);
  }

  base::CommandLine command_line_;
  IOThread::Globals globals_;
  std::string field_trial_group_;
  bool is_quic_allowed_by_policy_;
  std::map<std::string, std::string> field_trial_params_;
};

TEST_F(IOThreadTest, InitializeNetworkSessionParamsFromGlobals) {
  globals_.quic_connection_options.push_back(net::kTBBR);
  globals_.quic_connection_options.push_back(net::kTIME);

  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(globals_.quic_connection_options,
            params.quic_connection_options);
}

TEST_F(IOThreadTest, SpdyFieldTrialHoldbackEnabled) {
  net::HttpStreamFactory::set_spdy_enabled(true);
  field_trial_group_ = "SpdyDisabled";
  ConfigureSpdyGlobals();
  EXPECT_FALSE(net::HttpStreamFactory::spdy_enabled());
}

TEST_F(IOThreadTest, SpdyFieldTrialSpdy31Enabled) {
  field_trial_group_ = "Spdy31Enabled";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoSPDY31, net::kProtoHTTP11));
}

TEST_F(IOThreadTest, SpdyFieldTrialSpdy4Enabled) {
  field_trial_group_ = "Spdy4Enabled";
  ConfigureSpdyGlobals();
  EXPECT_THAT(
      globals_.next_protos,
      ElementsAre(net::kProtoHTTP2, net::kProtoSPDY31, net::kProtoHTTP11));
}

TEST_F(IOThreadTest, SpdyFieldTrialDefault) {
  field_trial_group_ = "";
  ConfigureSpdyGlobals();
  EXPECT_THAT(
      globals_.next_protos,
      ElementsAre(net::kProtoHTTP2, net::kProtoSPDY31, net::kProtoHTTP11));
}

TEST_F(IOThreadTest, SpdyFieldTrialParametrized) {
  field_trial_params_["enable_spdy31"] = "false";
  // Undefined parameter "enable_http2_14" should default to false.
  field_trial_params_["enable_http2"] = "true";
  field_trial_group_ = "ParametrizedHTTP2Only";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP2, net::kProtoHTTP11));
}

TEST_F(IOThreadTest, SpdyCommandLineUseSpdyOff) {
  command_line_.AppendSwitchASCII("use-spdy", "off");
  // Command line should overwrite field trial group.
  field_trial_group_ = "Spdy4Enabled";
  ConfigureSpdyGlobals();
  EXPECT_EQ(0u, globals_.next_protos.size());
}

TEST_F(IOThreadTest, NPNFieldTrialEnabled) {
  field_trial_group_ = "Enable-experiment";
  ConfigureNPNGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_npn);
}

TEST_F(IOThreadTest, NPNFieldTrialDisabled) {
  field_trial_group_ = "Disable-holdback";
  ConfigureNPNGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_npn);
}

TEST_F(IOThreadTest, DisableQuicByDefault) {
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_FALSE(params.enable_quic_for_proxies);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(IOThreadTest, EnableQuicFromFieldTrialGroup) {
  field_trial_group_ = "Enabled";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params default_params;
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_FALSE(params.disable_quic_on_timeout_with_open_streams);
  EXPECT_TRUE(params.enable_quic_for_proxies);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
  EXPECT_EQ(1.0, params.alternative_service_probability_threshold);
  EXPECT_EQ(default_params.quic_supported_versions,
            params.quic_supported_versions);
  EXPECT_EQ(net::QuicTagVector(), params.quic_connection_options);
  EXPECT_FALSE(params.quic_always_require_handshake_confirmation);
  EXPECT_FALSE(params.quic_disable_connection_pooling);
  EXPECT_EQ(0.25f, params.quic_load_server_info_timeout_srtt_multiplier);
  EXPECT_FALSE(params.quic_enable_connection_racing);
  EXPECT_FALSE(params.quic_enable_non_blocking_io);
  EXPECT_FALSE(params.quic_disable_disk_cache);
  EXPECT_FALSE(params.quic_prefer_aes);
  EXPECT_FALSE(params.parse_alternative_services);
  EXPECT_FALSE(params.enable_alternative_service_with_different_host);
  EXPECT_EQ(0, params.quic_max_number_of_lossy_connections);
  EXPECT_EQ(1.0f, params.quic_packet_loss_threshold);
  EXPECT_FALSE(params.quic_delay_tcp_race);
  EXPECT_FALSE(params.quic_close_sessions_on_ip_change);
  EXPECT_EQ(net::kIdleConnectionTimeoutSeconds,
            params.quic_idle_connection_timeout_seconds);
  EXPECT_FALSE(params.quic_disable_preconnect_if_0rtt);
  EXPECT_FALSE(params.quic_migrate_sessions_on_network_change);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
  EXPECT_TRUE(params.quic_host_whitelist.empty());
}

TEST_F(IOThreadTest,
       DisableQuicWhenConnectionTimesOutWithOpenStreamsFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["disable_quic_on_timeout_with_open_streams"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.disable_quic_on_timeout_with_open_streams);
}

TEST_F(IOThreadTest, EnableQuicFromQuicProxyFieldTrialGroup) {
  const struct {
    std::string field_trial_group_name;
    bool expect_enable_quic;
  } tests[] = {
      {
          std::string(), false,
      },
      {
          "NotEnabled", false,
      },
      {
          "Control", false,
      },
      {
          "Disabled", false,
      },
      {
          "EnabledControl", true,
      },
      {
          "Enabled", true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    base::FieldTrialList field_trial_list(new base::MockEntropyProvider());
    base::FieldTrialList::CreateFieldTrial(
        data_reduction_proxy::params::GetQuicFieldTrialName(),
        tests[i].field_trial_group_name);

    ConfigureQuicGlobals();
    net::HttpNetworkSession::Params params;
    InitializeNetworkSessionParams(&params);
    EXPECT_FALSE(params.enable_quic) << i;
    EXPECT_EQ(tests[i].expect_enable_quic, params.enable_quic_for_proxies) << i;
    EXPECT_EQ(tests[i].expect_enable_quic,
              IOThread::ShouldEnableQuicForDataReductionProxy())
        << i;
  }
}

TEST_F(IOThreadTest, EnableQuicFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_TRUE(params.enable_quic_for_proxies);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(IOThreadTest, AltSvcFieldTrialEnabled) {
  field_trial_group_ = "AltSvcEnabled";
  ConfigureAltSvcGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.parse_alternative_services);
  EXPECT_FALSE(params.enable_alternative_service_with_different_host);
}

TEST_F(IOThreadTest, AltSvcFieldTrialDisabled) {
  field_trial_group_ = "AltSvcDisabled";
  ConfigureAltSvcGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.parse_alternative_services);
  EXPECT_FALSE(params.enable_alternative_service_with_different_host);
}

TEST_F(IOThreadTest, EnableAlternativeServicesFromCommandLineWithQuicDisabled) {
  command_line_.AppendSwitch("enable-alternative-services");

  ConfigureAltSvcGlobals();
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_TRUE(params.parse_alternative_services);
  EXPECT_TRUE(params.enable_alternative_service_with_different_host);
}

TEST_F(IOThreadTest, EnableAlternativeServicesFromCommandLineWithQuicEnabled) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitch("enable-alternative-services");

  ConfigureAltSvcGlobals();
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_TRUE(params.parse_alternative_services);
  EXPECT_TRUE(params.enable_alternative_service_with_different_host);
}

TEST_F(IOThreadTest, PacketLengthFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-max-packet-length", "1450");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1450u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, QuicCloseSessionsOnIpChangeFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["close_sessions_on_ip_change"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_close_sessions_on_ip_change);
}

TEST_F(IOThreadTest, QuicIdleConnectionTimeoutSecondsFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["idle_connection_timeout_seconds"] = "300";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(300, params.quic_idle_connection_timeout_seconds);
}

TEST_F(IOThreadTest, QuicDisablePreConnectIfZeroRtt) {
  field_trial_group_ = "Enabled";
  field_trial_params_["disable_preconnect_if_0rtt"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_disable_preconnect_if_0rtt);
}

TEST_F(IOThreadTest, QuicMigrateSessionsOnNetworkChangeFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["migrate_sessions_on_network_change"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_migrate_sessions_on_network_change);
}

TEST_F(IOThreadTest, PacketLengthFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["max_packet_length"] = "1450";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1450u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, QuicVersionFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  std::string version =
      net::QuicVersionToString(net::QuicSupportedVersions().back());
  command_line_.AppendSwitchASCII("quic-version", version);

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions, params.quic_supported_versions);
}

TEST_F(IOThreadTest, QuicVersionFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["quic_version"] =
      net::QuicVersionToString(net::QuicSupportedVersions().back());

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions, params.quic_supported_versions);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-connection-options",
                                  "TIME,TBBR,REJ");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["connection_options"] = "TIME,TBBR,REJ";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest,
       QuicAlwaysRequireHandshakeConfirmationFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["always_require_handshake_confirmation"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_always_require_handshake_confirmation);
}

TEST_F(IOThreadTest,
       QuicDisableConnectionPoolingFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["disable_connection_pooling"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_disable_connection_pooling);
}

TEST_F(IOThreadTest, QuicLoadServerInfoTimeToSmoothedRttFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["load_server_info_time_to_srtt"] = "0.5";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(0.5f, params.quic_load_server_info_timeout_srtt_multiplier);
}

TEST_F(IOThreadTest, QuicEnableConnectionRacing) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_connection_racing"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_enable_connection_racing);
}

TEST_F(IOThreadTest, QuicEnableNonBlockingIO) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_non_blocking_io"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_enable_non_blocking_io);
}

TEST_F(IOThreadTest, QuicDisableDiskCache) {
  field_trial_group_ = "Enabled";
  field_trial_params_["disable_disk_cache"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_disable_disk_cache);
}

TEST_F(IOThreadTest, QuicPreferAes) {
  field_trial_group_ = "Enabled";
  field_trial_params_["prefer_aes"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_prefer_aes);
}

TEST_F(IOThreadTest, QuicEnableAlternativeServicesFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_alternative_service_with_different_host"] =
      "true";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_alternative_service_with_different_host);
}

TEST_F(IOThreadTest, QuicMaxNumberOfLossyConnectionsFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["max_number_of_lossy_connections"] = "5";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(5, params.quic_max_number_of_lossy_connections);
}

TEST_F(IOThreadTest, QuicPacketLossThresholdFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["packet_loss_threshold"] = "0.5";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(0.5f, params.quic_packet_loss_threshold);
}

TEST_F(IOThreadTest, QuicReceiveBufferSize) {
  field_trial_group_ = "Enabled";
  field_trial_params_["receive_buffer_size"] = "2097152";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(2097152, params.quic_socket_receive_buffer_size);
}

TEST_F(IOThreadTest, QuicDelayTcpConnection) {
  field_trial_group_ = "Enabled";
  field_trial_params_["delay_tcp_race"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_delay_tcp_race);
}

TEST_F(IOThreadTest, AlternativeServiceProbabilityThresholdFromFlag) {
  command_line_.AppendSwitchASCII("alternative-service-probability-threshold",
                                  "0.5");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(.5, params.alternative_service_probability_threshold);
}

TEST_F(IOThreadTest, AlternativeServiceProbabilityThresholdFromEnableQuicFlag) {
  command_line_.AppendSwitch("enable-quic");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(0, params.alternative_service_probability_threshold);
}

// TODO(bnc): Remove when new parameter name rolls out and server configuration
// is changed.
TEST_F(IOThreadTest, AlternativeServiceProbabilityThresholdFromOldParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["alternate_protocol_probability_threshold"] = ".5";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(.5, params.alternative_service_probability_threshold);
}

TEST_F(IOThreadTest, AlternativeServiceProbabilityThresholdFromParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["alternative_service_probability_threshold"] = ".5";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(.5, params.alternative_service_probability_threshold);
}

TEST_F(IOThreadTest, QuicWhitelistFromCommandLinet) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-host-whitelist",
                                  "www.example.org, www.example.com");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(2u, params.quic_host_whitelist.size());
  EXPECT_TRUE(ContainsKey(params.quic_host_whitelist, "www.example.org"));
  EXPECT_TRUE(ContainsKey(params.quic_host_whitelist, "www.example.com"));
}

TEST_F(IOThreadTest, QuicWhitelistFromParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["quic_host_whitelist"] =
      "www.example.org, www.example.com";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(2u, params.quic_host_whitelist.size());
  EXPECT_TRUE(ContainsKey(params.quic_host_whitelist, "www.example.org"));
  EXPECT_TRUE(ContainsKey(params.quic_host_whitelist, "www.example.com"));
}

TEST_F(IOThreadTest, QuicDisallowedByPolicy) {
  command_line_.AppendSwitch(switches::kEnableQuic);
  is_quic_allowed_by_policy_ = false;
  ConfigureQuicGlobals();

  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
}

class IOThreadTestWithIOThreadObject : public testing::Test {
 public:
  // These functions need to be public, since it is difficult to bind to
  // protected functions in a test (the code would need to explicitly contain
  // the name of the actual test class).
  void CheckCnameLookup(bool expected) {
    auto http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    ASSERT_NE(nullptr, http_auth_preferences);
    EXPECT_EQ(expected, http_auth_preferences->NegotiateDisableCnameLookup());
  }

  void CheckNegotiateEnablePort(bool expected) {
    auto http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    ASSERT_NE(nullptr, http_auth_preferences);
    EXPECT_EQ(expected, http_auth_preferences->NegotiateEnablePort());
  }

#if defined(OS_ANDROID)
  void CheckAuthAndroidNegoitateAccountType(std::string expected) {
    auto http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    ASSERT_NE(nullptr, http_auth_preferences);
    EXPECT_EQ(expected,
              http_auth_preferences->AuthAndroidNegotiateAccountType());
  }
#endif

  void CheckCanUseDefaultCredentials(bool expected, const GURL& url) {
    auto http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    EXPECT_EQ(expected, http_auth_preferences->CanUseDefaultCredentials(url));
  }

  void CheckCanDelegate(bool expected, const GURL& url) {
    auto http_auth_preferences =
        IOThreadPeer::GetAuthPreferences(io_thread_.get());
    EXPECT_EQ(expected, http_auth_preferences->CanDelegate(url));
  }

 protected:
  IOThreadTestWithIOThreadObject()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD |
                       content::TestBrowserThreadBundle::DONT_START_THREADS) {
#if defined(ENABLE_EXTENSIONS)
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
    io_thread_.reset(new IOThread(&pref_service_, &policy_service_,
                                         nullptr,
#if defined(ENABLE_EXTENSIONS)
                                         event_router_forwarder_.get()
#else
                                         nullptr
#endif
                                             ));
    // Now that IOThread object is registered starting the threads will
    // call the IOThread::Init(). This sets up the environment needed for
    // these tests.
    thread_bundle_.Start();
  }

  ~IOThreadTestWithIOThreadObject() override {
#if defined(USE_NSS_CERTS) || defined(OS_IOS)
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
#if defined(ENABLE_EXTENSIONS)
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
  scoped_ptr<IOThread> io_thread_;
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

}  // namespace test
