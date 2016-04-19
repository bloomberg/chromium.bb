// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/mock_entropy_provider.h"
#include "build/build_config.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert_net/nss_ocsp.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_network_session.h"
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

using ::testing::ReturnRef;

// Class used for accessing IOThread methods (friend of IOThread).
class IOThreadPeer {
 public:
  static net::HttpAuthPreferences* GetAuthPreferences(IOThread* io_thread) {
    return io_thread->globals()->http_auth_preferences.get();
  }
};

class NetworkSessionConfiguratorTest : public testing::Test {
 public:
  NetworkSessionConfiguratorTest()
      : is_spdy_allowed_by_policy_(true), is_quic_allowed_by_policy_(true) {
    field_trial_list_.reset(
        new base::FieldTrialList(new base::MockEntropyProvider()));
    variations::testing::ClearAllVariationParams();
  }

  void ParseFieldTrials() {
    network_session_configurator_.ParseFieldTrials(
        is_spdy_allowed_by_policy_, is_quic_allowed_by_policy_, &params_);
  }

  void ParseFieldTrialsAndCommandLine() {
    network_session_configurator_.ParseFieldTrialsAndCommandLine(
        is_spdy_allowed_by_policy_, is_quic_allowed_by_policy_, &params_);
  }

  bool is_spdy_allowed_by_policy_;
  bool is_quic_allowed_by_policy_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  net::HttpNetworkSession::Params params_;

 private:
  IOThread::NetworkSessionConfigurator network_session_configurator_;
};

TEST_F(NetworkSessionConfiguratorTest, Defaults) {
  ParseFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.ignore_certificate_errors);
  EXPECT_EQ(0u, params_.testing_fixed_http_port);
  EXPECT_EQ(0u, params_.testing_fixed_https_port);
  EXPECT_FALSE(params_.enable_spdy31);
  EXPECT_TRUE(params_.enable_http2);
  EXPECT_FALSE(params_.enable_tcp_fast_open_for_ssl);
  EXPECT_FALSE(params_.parse_alternative_services);
  EXPECT_FALSE(params_.enable_alternative_service_with_different_host);
  EXPECT_TRUE(params_.enable_npn);
  EXPECT_TRUE(params_.enable_priority_dependencies);
  EXPECT_FALSE(params_.enable_quic);
  EXPECT_FALSE(params_.enable_quic_for_proxies);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(NetworkSessionConfiguratorTest, IgnoreCertificateErrors) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "ignore-certificate-errors");

  ParseFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.ignore_certificate_errors);
}

TEST_F(NetworkSessionConfiguratorTest, TestingFixedPort) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "testing-fixed-http-port", "42");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "testing-fixed-https-port", "1234");

  ParseFieldTrialsAndCommandLine();

  EXPECT_EQ(42u, params_.testing_fixed_http_port);
  EXPECT_EQ(1234u, params_.testing_fixed_https_port);
}

TEST_F(NetworkSessionConfiguratorTest, AltSvcFieldTrialEnabled) {
  base::FieldTrialList::CreateFieldTrial("ParseAltSvc", "AltSvcEnabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.parse_alternative_services);
  EXPECT_FALSE(params_.enable_alternative_service_with_different_host);
}

TEST_F(NetworkSessionConfiguratorTest, AltSvcFieldTrialDisabled) {
  base::FieldTrialList::CreateFieldTrial("ParseAltSvc", "AltSvcDisabled");

  ParseFieldTrials();

  EXPECT_FALSE(params_.parse_alternative_services);
  EXPECT_FALSE(params_.enable_alternative_service_with_different_host);
}

TEST_F(NetworkSessionConfiguratorTest, SpdyFieldTrialHoldbackEnabled) {
  net::HttpStreamFactory::set_spdy_enabled(true);
  base::FieldTrialList::CreateFieldTrial("SPDY", "SpdyDisabled");

  ParseFieldTrials();

  EXPECT_FALSE(net::HttpStreamFactory::spdy_enabled());
}

TEST_F(NetworkSessionConfiguratorTest, SpdyFieldTrialSpdy31Enabled) {
  base::FieldTrialList::CreateFieldTrial("SPDY", "Spdy31Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_spdy31);
  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, SpdyFieldTrialSpdy4Enabled) {
  base::FieldTrialList::CreateFieldTrial("SPDY", "Spdy4Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_spdy31);
  EXPECT_TRUE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, SpdyFieldTrialParametrized) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_spdy31"] = "false";
  field_trial_params["enable_http2"] = "true";
  variations::AssociateVariationParams("SPDY", "ParametrizedHTTP2Only",
                                       field_trial_params);
  base::FieldTrialList::CreateFieldTrial("SPDY", "ParametrizedHTTP2Only");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_spdy31);
  EXPECT_TRUE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, SpdyCommandLineDisableHttp2) {
  // Command line should overwrite field trial group.
  base::CommandLine::ForCurrentProcess()->AppendSwitch("disable-http2");
  base::FieldTrialList::CreateFieldTrial("SPDY", "Spdy4Enabled");

  ParseFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_spdy31);
  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, SpdyDisallowedByPolicy) {
  is_spdy_allowed_by_policy_ = false;

  ParseFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_spdy31);
  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, NPNFieldTrialEnabled) {
  base::FieldTrialList::CreateFieldTrial("NPN", "Enable-experiment");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_npn);
}

TEST_F(NetworkSessionConfiguratorTest, NPNFieldTrialDisabled) {
  base::FieldTrialList::CreateFieldTrial("NPN", "Disable-holdback");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_npn);
}

TEST_F(NetworkSessionConfiguratorTest, PriorityDependenciesTrialEnabled) {
  base::FieldTrialList::CreateFieldTrial("SpdyEnableDependencies",
                                         "Enable-experiment");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_priority_dependencies);
}

TEST_F(NetworkSessionConfiguratorTest, PriorityDependenciesTrialDisabled) {
  base::FieldTrialList::CreateFieldTrial("SpdyEnableDependencies",
                                         "Disable-holdback");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_priority_dependencies);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromFieldTrialGroup) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_FALSE(params_.disable_quic_on_timeout_with_open_streams);
  EXPECT_TRUE(params_.enable_quic_for_proxies);
  EXPECT_EQ(1350u, params_.quic_max_packet_length);
  EXPECT_EQ(net::QuicTagVector(), params_.quic_connection_options);
  EXPECT_FALSE(params_.quic_always_require_handshake_confirmation);
  EXPECT_FALSE(params_.quic_disable_connection_pooling);
  EXPECT_EQ(0.25f, params_.quic_load_server_info_timeout_srtt_multiplier);
  EXPECT_FALSE(params_.quic_enable_connection_racing);
  EXPECT_FALSE(params_.quic_enable_non_blocking_io);
  EXPECT_FALSE(params_.quic_disable_disk_cache);
  EXPECT_FALSE(params_.quic_prefer_aes);
  EXPECT_FALSE(params_.parse_alternative_services);
  EXPECT_FALSE(params_.enable_alternative_service_with_different_host);
  EXPECT_EQ(0, params_.quic_max_number_of_lossy_connections);
  EXPECT_EQ(1.0f, params_.quic_packet_loss_threshold);
  EXPECT_FALSE(params_.quic_delay_tcp_race);
  EXPECT_FALSE(params_.quic_close_sessions_on_ip_change);
  EXPECT_EQ(net::kIdleConnectionTimeoutSeconds,
            params_.quic_idle_connection_timeout_seconds);
  EXPECT_FALSE(params_.quic_disable_preconnect_if_0rtt);
  EXPECT_FALSE(params_.quic_migrate_sessions_on_network_change);
  EXPECT_FALSE(params_.quic_migrate_sessions_early);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
  EXPECT_TRUE(params_.quic_host_whitelist.empty());

  net::HttpNetworkSession::Params default_params;
  EXPECT_EQ(default_params.quic_supported_versions,
            params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       DisableQuicWhenConnectionTimesOutWithOpenStreamsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_quic_on_timeout_with_open_streams"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.disable_quic_on_timeout_with_open_streams);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromQuicProxyFieldTrialGroup) {
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

  field_trial_list_.reset();
  for (size_t i = 0; i < arraysize(tests); ++i) {
    base::FieldTrialList field_trial_list(new base::MockEntropyProvider());
    base::FieldTrialList::CreateFieldTrial(
        data_reduction_proxy::params::GetQuicFieldTrialName(),
        tests[i].field_trial_group_name);

    ParseFieldTrials();

    EXPECT_FALSE(params_.enable_quic) << i;
    EXPECT_EQ(tests[i].expect_enable_quic, params_.enable_quic_for_proxies)
        << i;
    EXPECT_EQ(tests[i].expect_enable_quic,
              IOThread::ShouldEnableQuicForDataReductionProxy())
        << i;
  }
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");

  ParseFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_TRUE(params_.enable_quic_for_proxies);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(NetworkSessionConfiguratorTest,
       EnableAlternativeServicesFromCommandLineWithQuicDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "enable-alternative-services");

  ParseFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_quic);
  EXPECT_TRUE(params_.parse_alternative_services);
  EXPECT_TRUE(params_.enable_alternative_service_with_different_host);
}

TEST_F(NetworkSessionConfiguratorTest,
       EnableAlternativeServicesFromCommandLineWithQuicEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "enable-alternative-services");

  ParseFieldTrialsAndCommandLine();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_TRUE(params_.parse_alternative_services);
  EXPECT_TRUE(params_.enable_alternative_service_with_different_host);
}

TEST_F(NetworkSessionConfiguratorTest, PacketLengthFromCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "quic-max-packet-length", "1450");

  ParseFieldTrialsAndCommandLine();

  EXPECT_EQ(1450u, params_.quic_max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicCloseSessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["close_sessions_on_ip_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_close_sessions_on_ip_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicIdleConnectionTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["idle_connection_timeout_seconds"] = "300";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(300, params_.quic_idle_connection_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDisablePreConnectIfZeroRtt) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_preconnect_if_0rtt"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_disable_preconnect_if_0rtt);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsOnNetworkChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_on_network_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_migrate_sessions_on_network_change);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsEarlyFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_early"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_migrate_sessions_early);
}

TEST_F(NetworkSessionConfiguratorTest, PacketLengthFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_packet_length"] = "1450";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(1450u, params_.quic_max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");
  std::string version =
      net::QuicVersionToString(net::QuicSupportedVersions().back());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII("quic-version",
                                                            version);

  ParseFieldTrialsAndCommandLine();

  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] =
      net::QuicVersionToString(net::QuicSupportedVersions().back());
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest, QuicConnectionOptionsFromCommandLine) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "quic-connection-options", "TIME,TBBR,REJ");

  ParseFieldTrialsAndCommandLine();

  net::QuicTagVector options;
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params_.quic_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["connection_options"] = "TIME,TBBR,REJ";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  net::QuicTagVector options;
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params_.quic_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicAlwaysRequireHandshakeConfirmationFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["always_require_handshake_confirmation"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_always_require_handshake_confirmation);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicDisableConnectionPoolingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_connection_pooling"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_disable_connection_pooling);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicLoadServerInfoTimeToSmoothedRttFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["load_server_info_time_to_srtt"] = "0.5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(0.5f, params_.quic_load_server_info_timeout_srtt_multiplier);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEnableConnectionRacing) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_connection_racing"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_enable_connection_racing);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEnableNonBlockingIO) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_non_blocking_io"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_enable_non_blocking_io);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDisableDiskCache) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["disable_disk_cache"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_disable_disk_cache);
}

TEST_F(NetworkSessionConfiguratorTest, QuicPreferAes) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["prefer_aes"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_prefer_aes);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicEnableAlternativeServicesFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_alternative_service_with_different_host"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_alternative_service_with_different_host);
  // QUIC AltSvc pooling parameter should also enable AltSvc parsing.
  EXPECT_TRUE(params_.parse_alternative_services);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxNumberOfLossyConnectionsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_number_of_lossy_connections"] = "5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(5, params_.quic_max_number_of_lossy_connections);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicPacketLossThresholdFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["packet_loss_threshold"] = "0.5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(0.5f, params_.quic_packet_loss_threshold);
}

TEST_F(NetworkSessionConfiguratorTest, QuicReceiveBufferSize) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["receive_buffer_size"] = "2097152";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2097152, params_.quic_socket_receive_buffer_size);
}

TEST_F(NetworkSessionConfiguratorTest, QuicDelayTcpConnection) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["delay_tcp_race"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_delay_tcp_race);
}

TEST_F(NetworkSessionConfiguratorTest, QuicOriginsToForceQuicOn) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "origin-to-force-quic-on", "www.example.com:443, www.example.org:443");

  ParseFieldTrialsAndCommandLine();

  EXPECT_EQ(2u, params_.origins_to_force_quic_on.size());
  EXPECT_TRUE(
      ContainsKey(params_.origins_to_force_quic_on,
                  net::HostPortPair::FromString("www.example.com:443")));
  EXPECT_TRUE(
      ContainsKey(params_.origins_to_force_quic_on,
                  net::HostPortPair::FromString("www.example.org:443")));
}

TEST_F(NetworkSessionConfiguratorTest, QuicWhitelistFromCommandLinet) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch("enable-quic");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "quic-host-whitelist", "www.example.org, www.example.com");

  ParseFieldTrialsAndCommandLine();

  EXPECT_EQ(2u, params_.quic_host_whitelist.size());
  EXPECT_TRUE(ContainsKey(params_.quic_host_whitelist, "www.example.org"));
  EXPECT_TRUE(ContainsKey(params_.quic_host_whitelist, "www.example.com"));
}

TEST_F(NetworkSessionConfiguratorTest, QuicWhitelistFromParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_host_whitelist"] =
      "www.example.org, www.example.com";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2u, params_.quic_host_whitelist.size());
  EXPECT_TRUE(ContainsKey(params_.quic_host_whitelist, "www.example.org"));
  EXPECT_TRUE(ContainsKey(params_.quic_host_whitelist, "www.example.com"));
}

TEST_F(NetworkSessionConfiguratorTest, QuicDisallowedByPolicy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableQuic);
  is_quic_allowed_by_policy_ = false;

  ParseFieldTrialsAndCommandLine();

  EXPECT_FALSE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, TCPFastOpenHttpsEnabled) {
  base::FieldTrialList::CreateFieldTrial("TCPFastOpen", "HttpsEnabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_tcp_fast_open_for_ssl);
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
    io_thread_.reset(new IOThread(&pref_service_, &policy_service_, nullptr,
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

}  // namespace test
