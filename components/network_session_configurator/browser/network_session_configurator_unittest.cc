// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/browser/network_session_configurator.h"

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_features.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_stream_factory.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network_session_configurator {

class NetworkSessionConfiguratorTest : public testing::Test {
 public:
  NetworkSessionConfiguratorTest()
      : quic_user_agent_id_("Chrome/52.0.2709.0 Linux x86_64") {
    field_trial_list_.reset(new base::FieldTrialList(
        std::make_unique<base::MockEntropyProvider>()));
    variations::testing::ClearAllVariationParams();
  }

  void ParseCommandLineAndFieldTrials(const base::CommandLine& command_line) {
    network_session_configurator::ParseCommandLineAndFieldTrials(
        command_line,
        /*is_quic_force_disabled=*/false, quic_user_agent_id_, &params_);
  }

  void ParseFieldTrials() {
    ParseCommandLineAndFieldTrials(
        base::CommandLine(base::CommandLine::NO_PROGRAM));
  }

  std::string quic_user_agent_id_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  net::HttpNetworkSession::Params params_;
};

TEST_F(NetworkSessionConfiguratorTest, Defaults) {
  ParseFieldTrials();

  EXPECT_FALSE(params_.ignore_certificate_errors);
  EXPECT_EQ(0u, params_.testing_fixed_http_port);
  EXPECT_EQ(0u, params_.testing_fixed_https_port);
  EXPECT_EQ(params_.tcp_fast_open_mode,
            net::HttpNetworkSession::Params::TcpFastOpenMode::DISABLED);
  EXPECT_FALSE(params_.enable_user_alternate_protocol_ports);

  EXPECT_TRUE(params_.enable_http2);
  EXPECT_TRUE(params_.http2_settings.empty());
  EXPECT_FALSE(params_.enable_websocket_over_http2);

  EXPECT_FALSE(params_.enable_quic);
  EXPECT_EQ("Chrome/52.0.2709.0 Linux x86_64", params_.quic_user_agent_id);
  EXPECT_EQ(0u, params_.origins_to_force_quic_on.size());
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialGroupNameDoesNotMatter) {
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Disable");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, Http2FieldTrialDisable) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_enabled"] = "false";
  variations::AssociateVariationParams("HTTP2", "Experiment",
                                       field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Experiment");

  ParseFieldTrials();

  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromFieldTrialGroup) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
  EXPECT_FALSE(params_.mark_quic_broken_when_network_blackholes);
  EXPECT_TRUE(params_.retry_without_alt_svc_on_quic_errors);
  EXPECT_FALSE(params_.support_ietf_format_quic_altsvc);
  EXPECT_EQ(1350u, params_.quic_max_packet_length);
  EXPECT_EQ(quic::QuicTagVector(), params_.quic_connection_options);
  EXPECT_EQ(quic::QuicTagVector(), params_.quic_client_connection_options);
  EXPECT_FALSE(params_.enable_server_push_cancellation);
  EXPECT_FALSE(params_.quic_close_sessions_on_ip_change);
  EXPECT_FALSE(params_.quic_goaway_sessions_on_ip_change);
  EXPECT_EQ(net::kIdleConnectionTimeoutSeconds,
            params_.quic_idle_connection_timeout_seconds);
  EXPECT_EQ(quic::kPingTimeoutSecs, params_.quic_reduced_ping_timeout_seconds);
  EXPECT_EQ(quic::kMaxTimeForCryptoHandshakeSecs,
            params_.quic_max_time_before_crypto_handshake_seconds);
  EXPECT_EQ(quic::kInitialIdleTimeoutSecs,
            params_.quic_max_idle_time_before_crypto_handshake_seconds);
  EXPECT_FALSE(params_.quic_race_cert_verification);
  EXPECT_FALSE(params_.quic_estimate_initial_rtt);
  EXPECT_FALSE(params_.quic_migrate_sessions_on_network_change_v2);
  EXPECT_FALSE(params_.quic_migrate_sessions_early_v2);
  EXPECT_FALSE(params_.quic_go_away_on_path_degrading);
  EXPECT_FALSE(params_.quic_allow_server_migration);
  EXPECT_TRUE(params_.quic_host_whitelist.empty());

  net::HttpNetworkSession::Params default_params;
  EXPECT_EQ(default_params.quic_supported_versions,
            params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicFromParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_quic"] = "true";
  variations::AssociateVariationParams("QUIC", "UseQuic", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "UseQuic");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest, EnableQuicForDataReductionProxy) {
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  base::FieldTrialList::CreateFieldTrial("DataReductionProxyUseQuic",
                                         "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_quic);
}

TEST_F(NetworkSessionConfiguratorTest,
       MarkQuicBrokenWhenNetworkBlackholesFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["mark_quic_broken_when_network_blackholes"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.mark_quic_broken_when_network_blackholes);
}

TEST_F(NetworkSessionConfiguratorTest, DisableRetryWithoutAltSvcOnQuicErrors) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["retry_without_alt_svc_on_quic_errors"] = "false";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_FALSE(params_.retry_without_alt_svc_on_quic_errors);
}

TEST_F(NetworkSessionConfiguratorTest, SupportIetfFormatQuicAltSvc) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["support_ietf_format_quic_altsvc"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.support_ietf_format_quic_altsvc);
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
       QuicGoAwaySessionsOnIpChangeFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["goaway_sessions_on_ip_change"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_goaway_sessions_on_ip_change);
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

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "-5";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(quic::kPingTimeoutSecs, params_.quic_reduced_ping_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       LargeQuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "50";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(quic::kPingTimeoutSecs, params_.quic_reduced_ping_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicReducedPingTimeoutSecondsFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["reduced_ping_timeout_seconds"] = "10";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(10, params_.quic_reduced_ping_timeout_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_before_crypto_handshake_seconds"] = "7";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(7, params_.quic_max_time_before_crypto_handshake_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicMaxTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_before_crypto_handshake_seconds"] = "-1";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(quic::kMaxTimeForCryptoHandshakeSecs,
            params_.quic_max_time_before_crypto_handshake_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxIdleTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_idle_time_before_crypto_handshake_seconds"] = "11";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(11, params_.quic_max_idle_time_before_crypto_handshake_seconds);
}

TEST_F(NetworkSessionConfiguratorTest,
       NegativeQuicMaxIdleTimeBeforeCryptoHandshakeSeconds) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_idle_time_before_crypto_handshake_seconds"] = "-1";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");
  ParseFieldTrials();
  EXPECT_EQ(quic::kInitialIdleTimeoutSecs,
            params_.quic_max_idle_time_before_crypto_handshake_seconds);
}

TEST_F(NetworkSessionConfiguratorTest, QuicRaceCertVerification) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["race_cert_verification"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_race_cert_verification);
}

TEST_F(NetworkSessionConfiguratorTest, EnableServerPushCancellation) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["enable_server_push_cancellation"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_server_push_cancellation);
}

TEST_F(NetworkSessionConfiguratorTest, QuicEstimateInitialRtt) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["estimate_initial_rtt"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_estimate_initial_rtt);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsOnNetworkChangeV2FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_on_network_change_v2"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_migrate_sessions_on_network_change_v2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMigrateSessionsEarlyV2FromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["migrate_sessions_early_v2"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_migrate_sessions_early_v2);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicGoawayOnPathDegradingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["go_away_on_path_degrading"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_go_away_on_path_degrading);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicMaxTimeOnNonDefaultNetworkFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_time_on_non_default_network_seconds"] = "10";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            params_.quic_max_time_on_non_default_network);
}

TEST_F(
    NetworkSessionConfiguratorTest,
    QuicMaxNumMigrationsToNonDefaultNetworkOnWriteErrorFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_migrations_to_non_default_network_on_write_error"] =
      "3";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(3,
            params_.quic_max_migrations_to_non_default_network_on_write_error);
}

TEST_F(
    NetworkSessionConfiguratorTest,
    QuicMaxNumMigrationsToNonDefaultNetworkOnPathDegradingFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params
      ["max_migrations_to_non_default_network_on_path_degrading"] = "4";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(
      4, params_.quic_max_migrations_to_non_default_network_on_path_degrading);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicAllowServerMigrationFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["allow_server_migration"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_allow_server_migration);
}

TEST_F(NetworkSessionConfiguratorTest, PacketLengthFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["max_packet_length"] = "1450";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(1450u, params_.quic_max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersionFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["quic_version"] =
      quic::QuicVersionToString(quic::AllSupportedTransportVersions().back());
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTransportVersionVector supported_versions;
  supported_versions.push_back(quic::AllSupportedTransportVersions().back());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       MultipleQuicVersionFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  std::string quic_versions =
      quic::QuicVersionToString(quic::AllSupportedTransportVersions().front()) +
      "," +
      quic::QuicVersionToString(quic::AllSupportedTransportVersions().back());

  field_trial_params["quic_version"] = quic_versions;
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTransportVersionVector supported_versions;
  supported_versions.push_back(quic::AllSupportedTransportVersions().front());
  supported_versions.push_back(quic::AllSupportedTransportVersions().back());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest, SameQuicVersionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  std::string quic_versions =
      quic::QuicVersionToString(quic::AllSupportedTransportVersions().front()) +
      "," +
      quic::QuicVersionToString(quic::AllSupportedTransportVersions().front());

  field_trial_params["quic_version"] = quic_versions;
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTransportVersionVector supported_versions;
  supported_versions.push_back(quic::AllSupportedTransportVersions().front());
  EXPECT_EQ(supported_versions, params_.quic_supported_versions);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["connection_options"] = "TIME,TBBR,REJ";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTagVector options;
  options.push_back(quic::kTIME);
  options.push_back(quic::kTBBR);
  options.push_back(quic::kREJ);
  EXPECT_EQ(options, params_.quic_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest,
       QuicClientConnectionOptionsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["client_connection_options"] = "TBBR,1RTT";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  quic::QuicTagVector options;
  options.push_back(quic::kTBBR);
  options.push_back(quic::k1RTT);
  EXPECT_EQ(options, params_.quic_client_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest, QuicHostWhitelist) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["host_whitelist"] = "www.example.org, www.example.com";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_EQ(2u, params_.quic_host_whitelist.size());
  EXPECT_TRUE(
      base::ContainsKey(params_.quic_host_whitelist, "www.example.com"));
  EXPECT_TRUE(
      base::ContainsKey(params_.quic_host_whitelist, "www.example.org"));
}

TEST_F(NetworkSessionConfiguratorTest, QuicHostWhitelistEmpty) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["host_whitelist"] = "";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_host_whitelist.empty());
}

TEST_F(NetworkSessionConfiguratorTest, Http2SettingsFromFieldTrialParams) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["http2_settings"] = "7:1234,25:5678";
  variations::AssociateVariationParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  spdy::SettingsMap expected_settings;
  expected_settings[static_cast<spdy::SpdyKnownSettingsId>(7)] = 1234;
  expected_settings[static_cast<spdy::SpdyKnownSettingsId>(25)] = 5678;
  EXPECT_EQ(expected_settings, params_.http2_settings);
}

TEST_F(NetworkSessionConfiguratorTest, TCPFastOpenEnabled) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableTcpFastOpen);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(params_.tcp_fast_open_mode,
            net::HttpNetworkSession::Params::TcpFastOpenMode::ENABLED_FOR_ALL);
}

TEST_F(NetworkSessionConfiguratorTest, TCPFastOpenHttpsEnabled) {
  base::FieldTrialList::CreateFieldTrial("TCPFastOpen", "HttpsEnabled");

  ParseFieldTrials();
  EXPECT_EQ(
      params_.tcp_fast_open_mode,
      net::HttpNetworkSession::Params::TcpFastOpenMode::ENABLED_FOR_SSL_ONLY);

  // Make sure that the command line flag overrides the field trial.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableTcpFastOpen);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(params_.tcp_fast_open_mode,
            net::HttpNetworkSession::Params::TcpFastOpenMode::ENABLED_FOR_ALL);
}

TEST_F(NetworkSessionConfiguratorTest, ForceQuic) {
  struct {
    bool force_enabled;
    bool force_disabled;
    bool expect_quic_enabled;
  } kTests[] = {
      {true /* force_enabled */, false /* force_disabled */,
       true /* expect_quic_enabled */},
      {false /* force_enabled */, true /* force_disabled */,
       false /* expect_quic_enabled */},
      {true /* force_enabled */, true /* force_disabled */,
       false /* expect_quic_enabled */},
  };

  for (const auto& test : kTests) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (test.force_enabled)
      command_line.AppendSwitch(switches::kEnableQuic);
    if (test.force_disabled)
      command_line.AppendSwitch(switches::kDisableQuic);
    ParseCommandLineAndFieldTrials(command_line);
    EXPECT_EQ(test.expect_quic_enabled, params_.enable_quic);
  }
}

TEST_F(NetworkSessionConfiguratorTest, DisableHttp2) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kDisableHttp2);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_FALSE(params_.enable_http2);
}

TEST_F(NetworkSessionConfiguratorTest, QuicConnectionOptions) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicConnectionOptions,
                                 "TIMER,TBBR,REJ");
  ParseCommandLineAndFieldTrials(command_line);

  quic::QuicTagVector expected_options;
  expected_options.push_back(quic::kTIME);
  expected_options.push_back(quic::kTBBR);
  expected_options.push_back(quic::kREJ);
  EXPECT_EQ(expected_options, params_.quic_connection_options);
}

TEST_F(NetworkSessionConfiguratorTest, QuicMaxPacketLength) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kQuicMaxPacketLength, "42");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(42u, params_.quic_max_packet_length);
}

TEST_F(NetworkSessionConfiguratorTest, QuicVersion) {
  quic::QuicTransportVersionVector supported_versions =
      quic::AllSupportedTransportVersions();
  for (const auto& version : supported_versions) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kEnableQuic);
    command_line.AppendSwitchASCII(switches::kQuicVersion,
                                   quic::QuicVersionToString(version));
    ParseCommandLineAndFieldTrials(command_line);
    ASSERT_EQ(1u, params_.quic_supported_versions.size());
    EXPECT_EQ(version, params_.quic_supported_versions[0]);
  }
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "*");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(1u, params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u, params_.origins_to_force_quic_on.count(net::HostPortPair()));
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn2) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "foo:1234");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(1u, params_.origins_to_force_quic_on.size());
  EXPECT_EQ(1u, params_.origins_to_force_quic_on.count(
                    net::HostPortPair("foo", 1234)));
}

TEST_F(NetworkSessionConfiguratorTest, OriginToForceQuicOn3) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableQuic);
  command_line.AppendSwitchASCII(switches::kOriginToForceQuicOn, "foo:1,bar:2");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(2u, params_.origins_to_force_quic_on.size());
  EXPECT_EQ(
      1u, params_.origins_to_force_quic_on.count(net::HostPortPair("foo", 1)));
  EXPECT_EQ(
      1u, params_.origins_to_force_quic_on.count(net::HostPortPair("bar", 2)));
}

TEST_F(NetworkSessionConfiguratorTest, EnableUserAlternateProtocolPorts) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableUserAlternateProtocolPorts);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_TRUE(params_.enable_user_alternate_protocol_ports);
}

TEST_F(NetworkSessionConfiguratorTest, TestingFixedPorts) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kTestingFixedHttpPort, "800");
  command_line.AppendSwitchASCII(switches::kTestingFixedHttpsPort, "801");
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_EQ(800, params_.testing_fixed_http_port);
  EXPECT_EQ(801, params_.testing_fixed_https_port);
}

TEST_F(NetworkSessionConfiguratorTest, IgnoreCertificateErrors) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kIgnoreCertificateErrors);
  ParseCommandLineAndFieldTrials(command_line);
  EXPECT_TRUE(params_.ignore_certificate_errors);
}

TEST_F(NetworkSessionConfiguratorTest, HostRules) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kHostRules, "map *.com foo");
  ParseCommandLineAndFieldTrials(command_line);

  net::HostPortPair host_port_pair("spam.net", 80);
  EXPECT_FALSE(params_.host_mapping_rules.RewriteHost(&host_port_pair));
  EXPECT_EQ("spam.net", host_port_pair.host());

  host_port_pair = net::HostPortPair("spam.com", 80);
  EXPECT_TRUE(params_.host_mapping_rules.RewriteHost(&host_port_pair));
  EXPECT_EQ("foo", host_port_pair.host());
}

TEST_F(NetworkSessionConfiguratorTest, TokenBindingDisabled) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_FALSE(params_.enable_token_binding);
}

TEST_F(NetworkSessionConfiguratorTest, ChannelIDEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kChannelID);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_TRUE(params_.enable_channel_id);
}

TEST_F(NetworkSessionConfiguratorTest, ChannelIDDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kChannelID);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_FALSE(params_.enable_channel_id);
}

TEST_F(NetworkSessionConfiguratorTest, DefaultCacheBackend) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType(command_line));
#else
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
            ChooseCacheType(command_line));
#endif
}

TEST_F(NetworkSessionConfiguratorTest, UseSimpleCacheBackendOn) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kUseSimpleCacheBackend, "on");
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType(command_line));
}

TEST_F(NetworkSessionConfiguratorTest, UseSimpleCacheBackendOff) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kUseSimpleCacheBackend, "off");
#if !defined(OS_ANDROID)
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
            ChooseCacheType(command_line));
#else  // defined(OS_ANDROID)
  // Android always uses the simple cache.
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType(command_line));
#endif
}

TEST_F(NetworkSessionConfiguratorTest, SimpleCacheTrialExperimentYes) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::FieldTrialList::CreateFieldTrial("SimpleCacheTrial", "ExperimentYes");
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType(command_line));
}

TEST_F(NetworkSessionConfiguratorTest, SimpleCacheTrialDisable) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::FieldTrialList::CreateFieldTrial("SimpleCacheTrial", "Disable");
#if !defined(OS_ANDROID)
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_BLOCKFILE,
            ChooseCacheType(command_line));
#else  // defined(OS_ANDROID)
  // Android always uses the simple cache.
  EXPECT_EQ(net::URLRequestContextBuilder::HttpCacheParams::DISK_SIMPLE,
            ChooseCacheType(command_line));
#endif
}

TEST_F(NetworkSessionConfiguratorTest, QuicHeadersIncludeH2StreamDependency) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["headers_include_h2_stream_dependency"] = "true";
  variations::AssociateVariationParams("QUIC", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("QUIC", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.quic_headers_include_h2_stream_dependency);
}

TEST_F(NetworkSessionConfiguratorTest,
       WebsocketOverHttp2EnabledFromCommandLine) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableWebsocketOverHttp2);

  ParseCommandLineAndFieldTrials(command_line);

  EXPECT_TRUE(params_.enable_websocket_over_http2);
}

TEST_F(NetworkSessionConfiguratorTest,
       WebsocketOverHttp2EnabledFromFieldTrial) {
  std::map<std::string, std::string> field_trial_params;
  field_trial_params["websocket_over_http2"] = "true";
  variations::AssociateVariationParams("HTTP2", "Enabled", field_trial_params);
  base::FieldTrialList::CreateFieldTrial("HTTP2", "Enabled");

  ParseFieldTrials();

  EXPECT_TRUE(params_.enable_websocket_over_http2);
}

}  // namespace network_session_configurator
