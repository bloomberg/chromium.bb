// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_versions.h"

#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {
namespace {

class QuicVersionsTest : public QuicTest {
 protected:
  QuicVersionLabel MakeVersionLabel(char a, char b, char c, char d) {
    if (!FLAGS_quic_reloadable_flag_quic_use_net_byte_order_version_label) {
      return MakeQuicTag(a, b, c, d);
    }
    return MakeQuicTag(d, c, b, a);
  }
};

TEST_F(QuicVersionsTest, QuicVersionToQuicVersionLabel) {
// If you add a new version to the QuicTransportVersion enum you will need to
// add a new case to QuicVersionToQuicVersionLabel, otherwise this test will
// fail.

// TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  // Any logs would indicate an unsupported version which we don't expect.
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  log.StartCapturingLogs();
#endif

  // Explicitly test a specific version.
  if (!FLAGS_quic_reloadable_flag_quic_use_net_byte_order_version_label) {
    EXPECT_EQ(MakeQuicTag('Q', '0', '3', '5'),
              QuicVersionToQuicVersionLabel(QUIC_VERSION_35));
  } else {
    EXPECT_EQ(MakeQuicTag('5', '3', '0', 'Q'),
              QuicVersionToQuicVersionLabel(QUIC_VERSION_35));
  }

  // Loop over all supported versions and make sure that we never hit the
  // default case (i.e. all supported versions should be successfully converted
  // to valid QuicVersionLabels).
  for (size_t i = 0; i < arraysize(kSupportedTransportVersions); ++i) {
    QuicTransportVersion version = kSupportedTransportVersions[i];
    EXPECT_LT(0u, QuicVersionToQuicVersionLabel(version));
  }
}

TEST_F(QuicVersionsTest, QuicVersionToQuicVersionLabelUnsupported) {
// TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  // TODO(rjshade): Change to DFATAL once we actually support multiple versions,
  // and QuicConnectionTest::SendVersionNegotiationPacket can be changed to use
  // mis-matched versions rather than relying on QUIC_VERSION_UNSUPPORTED.
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(
      log, Log(base_logging::ERROR, _, "Unsupported QuicTransportVersion: 0"))
      .Times(1);
  log.StartCapturingLogs();
#endif

  EXPECT_EQ(0u, QuicVersionToQuicVersionLabel(QUIC_VERSION_UNSUPPORTED));
}

TEST_F(QuicVersionsTest, QuicVersionLabelToQuicTransportVersion) {
// If you add a new version to the QuicTransportVersion enum you will need to
// add a new case to QuicVersionLabelToQuicTransportVersion, otherwise this
// test will fail.

// TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  // Any logs would indicate an unsupported version which we don't expect.
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  log.StartCapturingLogs();
#endif

  // Explicitly test specific versions.
  if (!FLAGS_quic_reloadable_flag_quic_use_net_byte_order_version_label) {
    EXPECT_EQ(QUIC_VERSION_35,
              QuicVersionLabelToQuicVersion(MakeQuicTag('Q', '0', '3', '5')));
  } else {
    EXPECT_EQ(QUIC_VERSION_35,
              QuicVersionLabelToQuicVersion(MakeQuicTag('5', '3', '0', 'Q')));
  }

  for (size_t i = 0; i < arraysize(kSupportedTransportVersions); ++i) {
    QuicTransportVersion version = kSupportedTransportVersions[i];

    // Get the label from the version (we can loop over QuicVersions easily).
    QuicVersionLabel version_label = QuicVersionToQuicVersionLabel(version);
    EXPECT_LT(0u, version_label);

    // Now try converting back.
    QuicTransportVersion label_to_transport_version =
        QuicVersionLabelToQuicVersion(version_label);
    EXPECT_EQ(version, label_to_transport_version);
    EXPECT_NE(QUIC_VERSION_UNSUPPORTED, label_to_transport_version);
  }
}

TEST_F(QuicVersionsTest, QuicVersionLabelToQuicVersionUnsupported) {
// TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  ScopedMockLog log(kDoNotCaptureLogsYet);
#ifndef NDEBUG
  if (!FLAGS_quic_reloadable_flag_quic_use_net_byte_order_version_label) {
    EXPECT_CALL(log, Log(base_logging::INFO, _,
                         "Unsupported QuicVersionLabel version: FAKE"))
        .Times(1);
  } else {
    EXPECT_CALL(log, Log(base_logging::INFO, _,
                         "Unsupported QuicVersionLabel version: EKAF"))
        .Times(1);
  }
#endif
  log.StartCapturingLogs();
#endif

  EXPECT_EQ(QUIC_VERSION_UNSUPPORTED,
            QuicVersionLabelToQuicVersion(MakeQuicTag('F', 'A', 'K', 'E')));
}

TEST_F(QuicVersionsTest, QuicVersionLabelToHandshakeProtocol) {
// TODO(rtenneti): Enable checking of Log(ERROR) messages.
#if 0
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(_, _, _)).Times(0);
  log.StartCapturingLogs();
#endif

  for (size_t i = 0; i < arraysize(kSupportedTransportVersions); ++i) {
    QuicVersionLabel version_label =
        QuicVersionToQuicVersionLabel(kSupportedTransportVersions[i]);
    EXPECT_EQ(PROTOCOL_QUIC_CRYPTO,
              QuicVersionLabelToHandshakeProtocol(version_label));
  }

  // Test a TLS version:
  FLAGS_quic_supports_tls_handshake = true;
  QuicTag tls_tag;
  if (!FLAGS_quic_reloadable_flag_quic_use_net_byte_order_version_label) {
    tls_tag = MakeQuicTag('T', '0', '4', '1');
  } else {
    tls_tag = MakeQuicTag('1', '4', '0', 'T');
  }
  EXPECT_EQ(PROTOCOL_TLS1_3, QuicVersionLabelToHandshakeProtocol(tls_tag));

  FLAGS_quic_supports_tls_handshake = false;
#if 0
#ifndef NDEBUG
  EXPECT_CALL(log, Log(base_logging::INFO, _,
                       "Unsupported QuicVersionLabel version: T041"))
      .Times(1);
#endif
#endif
  EXPECT_EQ(PROTOCOL_UNSUPPORTED, QuicVersionLabelToHandshakeProtocol(tls_tag));
}

TEST_F(QuicVersionsTest, ParseQuicVersionLabel) {
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_37),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '3', '7')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_38),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '3', '8')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_39),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_41),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '1')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_42),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '2')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '3')));

  // Test a TLS version:
  FLAGS_quic_supports_tls_handshake = true;
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_35),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_37),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '7')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_38),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '8')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_39),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_41),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '1')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_42),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '2')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_43),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '3')));

  FLAGS_quic_supports_tls_handshake = false;
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '7')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '8')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '1')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '2')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '3')));
}

TEST_F(QuicVersionsTest, CreateQuicVersionLabel) {
  EXPECT_EQ(MakeVersionLabel('Q', '0', '3', '5'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '3', '7'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_37)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '3', '8'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_38)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '3', '9'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_39)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '1'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_41)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '2'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_42)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '3'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43)));

  // Test a TLS version:
  FLAGS_quic_supports_tls_handshake = true;
  EXPECT_EQ(MakeVersionLabel('T', '0', '3', '5'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_35)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '3', '7'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_37)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '3', '8'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_38)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '3', '9'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_39)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '1'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_41)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '2'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_42)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '3'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_43)));

  FLAGS_quic_supports_tls_handshake = false;
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '7')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '8')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '1')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '2')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '3')));
}

TEST_F(QuicVersionsTest, QuicVersionToString) {
  EXPECT_EQ("QUIC_VERSION_35", QuicVersionToString(QUIC_VERSION_35));
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED",
            QuicVersionToString(QUIC_VERSION_UNSUPPORTED));

  QuicTransportVersion single_version[] = {QUIC_VERSION_35};
  QuicTransportVersionVector versions_vector;
  for (size_t i = 0; i < arraysize(single_version); ++i) {
    versions_vector.push_back(single_version[i]);
  }
  EXPECT_EQ("QUIC_VERSION_35",
            QuicTransportVersionVectorToString(versions_vector));

  QuicTransportVersion multiple_versions[] = {QUIC_VERSION_UNSUPPORTED,
                                              QUIC_VERSION_35};
  versions_vector.clear();
  for (size_t i = 0; i < arraysize(multiple_versions); ++i) {
    versions_vector.push_back(multiple_versions[i]);
  }
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED,QUIC_VERSION_35",
            QuicTransportVersionVectorToString(versions_vector));

  // Make sure that all supported versions are present in QuicVersionToString.
  for (size_t i = 0; i < arraysize(kSupportedTransportVersions); ++i) {
    QuicTransportVersion version = kSupportedTransportVersions[i];
    EXPECT_NE("QUIC_VERSION_UNSUPPORTED", QuicVersionToString(version));
  }
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo38) {
  QuicTransportVersionVector all_versions = {QUIC_VERSION_35, QUIC_VERSION_37,
                                             QUIC_VERSION_38, QUIC_VERSION_39};

  FLAGS_quic_reloadable_flag_quic_enable_version_38 = false;

  QuicTransportVersionVector filtered_versions =
      FilterSupportedTransportVersions(all_versions);
  ASSERT_EQ(2u, filtered_versions.size());
  EXPECT_EQ(QUIC_VERSION_35, filtered_versions[0]);
  EXPECT_EQ(QUIC_VERSION_37, filtered_versions[1]);
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo39) {
  QuicTransportVersionVector all_versions = {QUIC_VERSION_35, QUIC_VERSION_37,
                                             QUIC_VERSION_38, QUIC_VERSION_39};

  FLAGS_quic_reloadable_flag_quic_enable_version_38 = true;
  FLAGS_quic_reloadable_flag_quic_enable_version_39 = false;

  QuicTransportVersionVector filtered_versions =
      FilterSupportedTransportVersions(all_versions);
  ASSERT_EQ(3u, filtered_versions.size());
  EXPECT_EQ(QUIC_VERSION_35, filtered_versions[0]);
  EXPECT_EQ(QUIC_VERSION_37, filtered_versions[1]);
  EXPECT_EQ(QUIC_VERSION_38, filtered_versions[2]);
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsAllVersions) {
  QuicTransportVersionVector all_versions = {QUIC_VERSION_35, QUIC_VERSION_37,
                                             QUIC_VERSION_38, QUIC_VERSION_39};

  FLAGS_quic_reloadable_flag_quic_enable_version_38 = true;
  FLAGS_quic_reloadable_flag_quic_enable_version_39 = true;

  QuicTransportVersionVector filtered_versions =
      FilterSupportedTransportVersions(all_versions);
  ASSERT_EQ(all_versions, filtered_versions);
}

TEST_F(QuicVersionsTest, LookUpVersionByIndex) {
  QuicTransportVersionVector all_versions = {QUIC_VERSION_35, QUIC_VERSION_37,
                                             QUIC_VERSION_38, QUIC_VERSION_39};
  int version_count = all_versions.size();
  for (int i = -5; i <= version_count + 1; ++i) {
    if (i >= 0 && i < version_count) {
      EXPECT_EQ(all_versions[i], VersionOfIndex(all_versions, i)[0]);
    } else {
      EXPECT_EQ(QUIC_VERSION_UNSUPPORTED, VersionOfIndex(all_versions, i)[0]);
    }
  }
}
// This test may appear to be so simplistic as to be unnecessary,
// yet a typo was made in doing the #defines and it was caught
// only in some test far removed from here... Better safe than sorry.
TEST_F(QuicVersionsTest, CheckVersionNumbersForTypos) {
  static_assert(arraysize(net::kSupportedTransportVersions) == 7u,
                "Supported versions out of sync");
  EXPECT_EQ(QUIC_VERSION_35, 35);
  EXPECT_EQ(QUIC_VERSION_37, 37);
  EXPECT_EQ(QUIC_VERSION_38, 38);
  EXPECT_EQ(QUIC_VERSION_39, 39);
  EXPECT_EQ(QUIC_VERSION_41, 41);
  EXPECT_EQ(QUIC_VERSION_42, 42);
  EXPECT_EQ(QUIC_VERSION_43, 43);
}
}  // namespace
}  // namespace test
}  // namespace net
