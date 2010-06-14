// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data.h"

#include "chrome/browser/sync/glue/extension_util.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class ExtensionDataTest : public testing::Test {
};

const char kValidId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kValidUpdateUrl1[] = "http://www.google.com";
const char kValidUpdateUrl2[] = "http://www.bing.com";
const char kValidVersion1[] = "1.0.0.0";
const char kValidVersion2[] = "1.1.0.0";

TEST_F(ExtensionDataTest, FromData) {
  sync_pb::ExtensionSpecifics client_data;
  client_data.set_id(kValidId);
  client_data.set_update_url(kValidUpdateUrl1);
  client_data.set_version(kValidVersion1);
  ExtensionData extension_data =
      ExtensionData::FromData(ExtensionData::CLIENT, client_data);
  EXPECT_FALSE(extension_data.NeedsUpdate(ExtensionData::CLIENT));
  EXPECT_TRUE(extension_data.NeedsUpdate(ExtensionData::SERVER));
  EXPECT_TRUE(AreExtensionSpecificsEqual(
      client_data, extension_data.merged_data()));
}

TEST_F(ExtensionDataTest, SetData) {
  sync_pb::ExtensionSpecifics client_data;
  client_data.set_id(kValidId);
  client_data.set_update_url(kValidUpdateUrl1);
  client_data.set_version(kValidVersion1);
  ExtensionData extension_data =
      ExtensionData::FromData(ExtensionData::CLIENT, client_data);

  {
    sync_pb::ExtensionSpecifics server_data;
    server_data.set_id(kValidId);
    server_data.set_update_url(kValidUpdateUrl2);
    server_data.set_version(kValidVersion2);
    server_data.set_enabled(true);
    extension_data.SetData(ExtensionData::SERVER, false, server_data);
    EXPECT_TRUE(extension_data.NeedsUpdate(ExtensionData::CLIENT));
    EXPECT_TRUE(extension_data.NeedsUpdate(ExtensionData::SERVER));
  }

  {
    sync_pb::ExtensionSpecifics server_data;
    server_data.set_id(kValidId);
    server_data.set_update_url(kValidUpdateUrl2);
    server_data.set_version(kValidVersion2);
    server_data.set_enabled(true);
    extension_data.SetData(ExtensionData::SERVER, true, server_data);
    EXPECT_TRUE(extension_data.NeedsUpdate(ExtensionData::CLIENT));
    EXPECT_FALSE(extension_data.NeedsUpdate(ExtensionData::SERVER));
    EXPECT_TRUE(AreExtensionSpecificsEqual(
        server_data, extension_data.merged_data()));
  }
}

TEST_F(ExtensionDataTest, ResolveData) {
  sync_pb::ExtensionSpecifics client_data;
  client_data.set_id(kValidId);
  client_data.set_update_url(kValidUpdateUrl1);
  client_data.set_version(kValidVersion1);
  ExtensionData extension_data =
      ExtensionData::FromData(ExtensionData::CLIENT, client_data);

  sync_pb::ExtensionSpecifics server_data;
  server_data.set_id(kValidId);
  server_data.set_update_url(kValidUpdateUrl2);
  server_data.set_version(kValidVersion2);
  extension_data.SetData(ExtensionData::SERVER, true, server_data);

  extension_data.ResolveData(ExtensionData::CLIENT);
  EXPECT_FALSE(extension_data.NeedsUpdate(ExtensionData::CLIENT));
  EXPECT_FALSE(extension_data.NeedsUpdate(ExtensionData::SERVER));
  EXPECT_TRUE(AreExtensionSpecificsEqual(
      server_data, extension_data.merged_data()));
}

}  // namespace

}  // namespace browser_sync
