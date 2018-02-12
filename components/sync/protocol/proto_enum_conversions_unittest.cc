// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/proto_enum_conversions.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

// Keep this file in sync with the .proto files in this directory.
class ProtoEnumConversionsTest : public testing::Test {};

template <class T>
void TestEnumStringFunction(T enum_min, T enum_max) {
  for (int i = enum_min; i <= enum_max; ++i) {
    const std::string& str = ProtoEnumToString(static_cast<T>(i));
    EXPECT_FALSE(str.empty());
  }
}

TEST_F(ProtoEnumConversionsTest, GetAppListItemTypeString) {
  TestEnumStringFunction(sync_pb::AppListSpecifics::AppListItemType_MIN,
                         sync_pb::AppListSpecifics::AppListItemType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetBrowserTypeString) {
  TestEnumStringFunction(sync_pb::SessionWindow::BrowserType_MIN,
                         sync_pb::SessionWindow::BrowserType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionString) {
  TestEnumStringFunction(sync_pb::SyncEnums::PageTransition_MIN,
                         sync_pb::SyncEnums::PageTransition_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetPageTransitionQualifierString) {
  TestEnumStringFunction(sync_pb::SyncEnums::PageTransitionRedirectType_MIN,
                         sync_pb::SyncEnums::PageTransitionRedirectType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetWifiCredentialSecurityClassString) {
  TestEnumStringFunction(sync_pb::WifiCredentialSpecifics::SecurityClass_MIN,
                         sync_pb::WifiCredentialSpecifics::SecurityClass_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetUpdatesSourceString) {
  TestEnumStringFunction(sync_pb::GetUpdatesCallerInfo::GetUpdatesSource_MIN,
                         sync_pb::GetUpdatesCallerInfo::PERIODIC);
  TestEnumStringFunction(sync_pb::GetUpdatesCallerInfo::RETRY,
                         sync_pb::GetUpdatesCallerInfo::GetUpdatesSource_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetResponseTypeString) {
  TestEnumStringFunction(sync_pb::CommitResponse::ResponseType_MIN,
                         sync_pb::CommitResponse::ResponseType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetErrorTypeString) {
  // We have a gap, so we need to do two ranges.
  TestEnumStringFunction(sync_pb::SyncEnums::ErrorType_MIN,
                         sync_pb::SyncEnums::MIGRATION_DONE);
  TestEnumStringFunction(sync_pb::SyncEnums::UNKNOWN,
                         sync_pb::SyncEnums::ErrorType_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetActionString) {
  TestEnumStringFunction(sync_pb::SyncEnums::Action_MIN,
                         sync_pb::SyncEnums::Action_MAX);
}

TEST_F(ProtoEnumConversionsTest, GetUserEventSpecificsString) {
  TestEnumStringFunction(
      sync_pb::UserEventSpecifics::UserConsent::CONSENT_STATUS_UNSPECIFIED,
      sync_pb::UserEventSpecifics::UserConsent::GIVEN);
  TestEnumStringFunction(
      sync_pb::UserEventSpecifics::UserConsent::FEATURE_UNSPECIFIED,
      sync_pb::UserEventSpecifics::UserConsent::CHROME_SYNC);
}

}  // namespace
}  // namespace syncer
