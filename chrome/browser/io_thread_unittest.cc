// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/io_thread.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/quic/quic_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

using ::testing::ElementsAre;

class IOThreadPeer {
 public:
  static void ConfigureQuicGlobals(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const std::map<std::string, std::string>& quic_trial_params,
      IOThread::Globals* globals) {
    IOThread::ConfigureQuicGlobals(command_line, quic_trial_group,
                                   quic_trial_params, globals);
  }

  static void InitializeNetworkSessionParamsFromGlobals(
      const IOThread::Globals& globals,
      net::HttpNetworkSession::Params* params) {
    IOThread::InitializeNetworkSessionParamsFromGlobals(globals, params);
  }

  static void ConfigureSpdyFromTrial(const std::string& trial_group,
                                     IOThread::Globals* globals) {
    IOThread::ConfigureSpdyFromTrial(trial_group, globals);
  }
};

class IOThreadTest : public testing::Test {
 public:
  IOThreadTest() : command_line_(base::CommandLine::NO_PROGRAM) {
    globals_.http_server_properties.reset(new net::HttpServerPropertiesImpl());
  }

  void ConfigureQuicGlobals() {
    IOThreadPeer::ConfigureQuicGlobals(command_line_, field_trial_group_,
                                       field_trial_params_, &globals_);
  }

  void InitializeNetworkSessionParams(net::HttpNetworkSession::Params* params) {
    IOThreadPeer::InitializeNetworkSessionParamsFromGlobals(globals_, params);
  }

  base::CommandLine command_line_;
  IOThread::Globals globals_;
  std::string field_trial_group_;
  std::map<std::string, std::string> field_trial_params_;
};

TEST_F(IOThreadTest, InitializeNetworkSessionParamsFromGlobals) {
  globals_.quic_connection_options.push_back(net::kPACE);
  globals_.quic_connection_options.push_back(net::kTBBR);
  globals_.quic_connection_options.push_back(net::kTIME);

  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(globals_.quic_connection_options,
            params.quic_connection_options);
}

TEST_F(IOThreadTest, SpdyFieldTrialHoldbackEnabled) {
  net::HttpStreamFactory::set_spdy_enabled(true);
  IOThreadPeer::ConfigureSpdyFromTrial("SpdyDisabled", &globals_);
  EXPECT_FALSE(net::HttpStreamFactory::spdy_enabled());
}

TEST_F(IOThreadTest, SpdyFieldTrialHoldbackControl) {
  bool use_alternate_protocols = false;
  IOThreadPeer::ConfigureSpdyFromTrial("Control", &globals_);
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11,
                          net::kProtoQUIC1SPDY3,
                          net::kProtoSPDY3,
                          net::kProtoSPDY31));
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyFieldTrialSpdy4Enabled) {
  bool use_alternate_protocols = false;
  IOThreadPeer::ConfigureSpdyFromTrial("Spdy4Enabled", &globals_);
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11,
                          net::kProtoQUIC1SPDY3,
                          net::kProtoSPDY3,
                          net::kProtoSPDY31,
                          net::kProtoSPDY4));
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyFieldTrialSpdy4Control) {
  bool use_alternate_protocols = false;
  IOThreadPeer::ConfigureSpdyFromTrial("Spdy4Control", &globals_);
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11,
                          net::kProtoQUIC1SPDY3,
                          net::kProtoSPDY3,
                          net::kProtoSPDY31));
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, DisableQuicByDefault) {
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
}

TEST_F(IOThreadTest, EnableQuicFromFieldTrialGroup) {
  field_trial_group_ = "Enabled";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params default_params;
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_FALSE(params.enable_quic_time_based_loss_detection);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
  EXPECT_EQ(default_params.quic_supported_versions,
            params.quic_supported_versions);
  EXPECT_EQ(net::QuicTagVector(), params.quic_connection_options);
  EXPECT_FALSE(params.quic_always_require_handshake_confirmation);
}

TEST_F(IOThreadTest, EnableQuicFromCommandLine) {
  base::CommandLine::StringVector args;
  command_line_.AppendSwitch("enable-quic");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
}

TEST_F(IOThreadTest, EnablePacingFromCommandLine) {
  base::CommandLine::StringVector args;
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitch("enable-quic-pacing");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicTagVector options;
  options.push_back(net::kPACE);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, EnablePacingFromFieldTrialGroup) {
  field_trial_group_ = "EnabledWithPacing";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicTagVector options;
  options.push_back(net::kPACE);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, EnablePacingFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_pacing"] = "true";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicTagVector options;
  options.push_back(net::kPACE);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, EnableTimeBasedLossDetectionFromCommandLine) {
  base::CommandLine::StringVector args;
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitch("enable-quic-time-based-loss-detection");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic_time_based_loss_detection);
}

TEST_F(IOThreadTest, EnableTimeBasedLossDetectionFromFieldTrialGroup) {
  field_trial_group_ = "EnabledWithTimeBasedLossDetection";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic_time_based_loss_detection);
}

TEST_F(IOThreadTest, EnableTimeBasedLossDetectionFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_time_based_loss_detection"] = "true";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic_time_based_loss_detection);
}

TEST_F(IOThreadTest, PacketLengthFromCommandLine) {
  base::CommandLine::StringVector args;
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-max-packet-length", "1350");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, PacketLengthFromFieldTrialGroup) {
  field_trial_group_ = "Enabled1350BytePackets";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, PacketLengthFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["max_packet_length"] = "1350";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, QuicVersionFromCommandLine) {
  base::CommandLine::StringVector args;
  command_line_.AppendSwitch("enable-quic");
  std::string version =
      net::QuicVersionToString(net::QuicSupportedVersions().back());
  command_line_.AppendSwitchASCII("quic-version", version);

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions,
            params.quic_supported_versions);
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
  EXPECT_EQ(supported_versions,
            params.quic_supported_versions);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromCommandLine) {
  base::CommandLine::StringVector args;
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-connection-options",
                                  "PACE,TIME,TBBR,REJ");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kPACE);
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["connection_options"] = "PACE,TIME,TBBR,REJ";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kPACE);
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromDeprecatedFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["congestion_options"] = "PACE,TIME,TBBR,REJ";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kPACE);
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

}  // namespace test
