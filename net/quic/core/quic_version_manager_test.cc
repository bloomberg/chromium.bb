// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_version_manager.h"

#include "net/quic/core/quic_versions.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {
namespace {

class QuicVersionManagerTest : public QuicTest {};

TEST_F(QuicVersionManagerTest, QuicVersionManager) {
  SetQuicFlag(&FLAGS_quic_enable_version_43, false);
  SetQuicFlag(&FLAGS_quic_enable_version_42, false);
  FLAGS_quic_reloadable_flag_quic_enable_version_41 = false;
  FLAGS_quic_reloadable_flag_quic_enable_version_39 = false;
  FLAGS_quic_reloadable_flag_quic_enable_version_38 = false;
  QuicVersionManager manager(AllSupportedTransportVersions());
  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());

  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());
  ASSERT_EQ(2u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[1]);

  FLAGS_quic_reloadable_flag_quic_enable_version_38 = true;
  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());
  ASSERT_EQ(3u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[2]);

  FLAGS_quic_reloadable_flag_quic_enable_version_39 = true;
  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());
  ASSERT_EQ(4u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[3]);

  FLAGS_quic_reloadable_flag_quic_enable_version_41 = true;
  ASSERT_EQ(5u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[4]);

  SetQuicFlag(&FLAGS_quic_enable_version_42, true);
  ASSERT_EQ(6u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_42, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[4]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[5]);

  // enable version 43; ensure that all versions are now supported..
  SetQuicFlag(&FLAGS_quic_enable_version_43, true);
  ASSERT_EQ(7u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_43, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_42, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[4]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[5]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[6]);

  EXPECT_EQ(FilterSupportedTransportVersions(AllSupportedTransportVersions()),
            manager.GetSupportedTransportVersions());
  ASSERT_EQ(7u, manager.GetSupportedTransportVersions().size());
  EXPECT_EQ(QUIC_VERSION_43, manager.GetSupportedTransportVersions()[0]);
  EXPECT_EQ(QUIC_VERSION_42, manager.GetSupportedTransportVersions()[1]);
  EXPECT_EQ(QUIC_VERSION_41, manager.GetSupportedTransportVersions()[2]);
  EXPECT_EQ(QUIC_VERSION_39, manager.GetSupportedTransportVersions()[3]);
  EXPECT_EQ(QUIC_VERSION_38, manager.GetSupportedTransportVersions()[4]);
  EXPECT_EQ(QUIC_VERSION_37, manager.GetSupportedTransportVersions()[5]);
  EXPECT_EQ(QUIC_VERSION_35, manager.GetSupportedTransportVersions()[6]);
}

}  // namespace
}  // namespace test
}  // namespace net
