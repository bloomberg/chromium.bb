// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_version_manager.h"

#include "net/quic/core/quic_versions.h"
#include "net/quic/platform/api/quic_arraysize.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {
namespace {

class QuicVersionManagerTest : public QuicTest {};

TEST_F(QuicVersionManagerTest, QuicVersionManager) {
  static_assert(QUIC_ARRAYSIZE(kSupportedTransportVersions) == 8u,
                "Supported versions out of sync");
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  SetQuicFlag(&FLAGS_quic_enable_version_43, false);
  SetQuicReloadableFlag(quic_enable_version_42, false);
  QuicVersionManager manager(AllSupportedVersions());

  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());
  ASSERT_EQ(5u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[4]);

  SetQuicReloadableFlag(quic_enable_version_42, true);
  SetQuicReloadableFlag(quic_allow_receiving_overlapping_data, true);
  ASSERT_EQ(6u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_42, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[4]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[5]);

  SetQuicFlag(&FLAGS_quic_enable_version_43, true);
  ASSERT_EQ(7u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_43, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_42, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[4]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[5]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[6]);

  SetQuicFlag(&FLAGS_quic_enable_version_99, true);
  ASSERT_EQ(8u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_99, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_43, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_42, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[4]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[5]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[6]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[7]);

  // Ensure that all versions are now supported.
  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());
}

}  // namespace
}  // namespace test
}  // namespace net
