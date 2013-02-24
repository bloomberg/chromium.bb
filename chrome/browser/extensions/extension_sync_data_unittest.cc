// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "googleurl/src/gurl.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(OS_WIN)
const base::FilePath::CharType kExtensionFilePath[] =
    FILE_PATH_LITERAL("c:\\foo");
#elif defined(OS_POSIX)
const base::FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("/foo");
#endif

const char kValidId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kValidVersion[] = "0.0.0.0";
const char kVersion1[] = "1.0.0.1";
const char kVersion2[] = "1.0.1.0";
const char kVersion3[] = "1.1.0.0";
const char kValidUpdateUrl1[] =
    "http://clients2.google.com/service/update2/crx";
const char kValidUpdateUrl2[] =
    "https://clients2.google.com/service/update2/crx";
const char kName[] = "MyExtension";
const char kName2[] = "MyExtension2";

class ExtensionSyncDataTest : public testing::Test {
};

TEST_F(ExtensionSyncDataTest, SyncDataToExtensionSyncDataForExtension) {
  sync_pb::EntitySpecifics entity;
  sync_pb::ExtensionSpecifics* extension_specifics = entity.mutable_extension();
  extension_specifics->set_id(kValidId);
  extension_specifics->set_update_url(kValidUpdateUrl2);
  extension_specifics->set_enabled(false);
  extension_specifics->set_incognito_enabled(true);
  extension_specifics->set_version(kVersion1);
  extension_specifics->set_name(kName);
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);

  extensions::ExtensionSyncData extension_sync_data(sync_data);
  EXPECT_EQ(extension_specifics->id(), extension_sync_data.id());
  EXPECT_EQ(extension_specifics->version(),
            extension_sync_data.version().GetString());
  EXPECT_EQ(extension_specifics->update_url(),
            extension_sync_data.update_url().spec());
  EXPECT_EQ(extension_specifics->enabled(), extension_sync_data.enabled());
  EXPECT_EQ(extension_specifics->incognito_enabled(),
            extension_sync_data.incognito_enabled());
  EXPECT_EQ(extension_specifics->name(), extension_sync_data.name());
  EXPECT_FALSE(extension_sync_data.uninstalled());
}

TEST_F(ExtensionSyncDataTest, ExtensionSyncDataToSyncDataForExtension) {
  sync_pb::EntitySpecifics entity;
  sync_pb::ExtensionSpecifics* input_extension = entity.mutable_extension();
  input_extension->set_id(kValidId);
  input_extension->set_update_url(kValidUpdateUrl2);
  input_extension->set_enabled(true);
  input_extension->set_incognito_enabled(false);
  input_extension->set_version(kVersion1);
  input_extension->set_name(kName);
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData("sync_tag", "non_unique_title", entity);
  extensions::ExtensionSyncData extension_sync_data(sync_data);

  syncer::SyncData output_sync_data = extension_sync_data.GetSyncData();
  const sync_pb::ExtensionSpecifics& output_specifics =
      output_sync_data.GetSpecifics().extension();
  EXPECT_EQ(extension_sync_data.id(), output_specifics.id());
  EXPECT_EQ(extension_sync_data.update_url().spec(),
            output_specifics.update_url());
  EXPECT_EQ(extension_sync_data.enabled(), output_specifics.enabled());
  EXPECT_EQ(extension_sync_data.incognito_enabled(),
            output_specifics.incognito_enabled());
  EXPECT_EQ(extension_sync_data.version().GetString(),
            output_specifics.version());
  EXPECT_EQ(extension_sync_data.name(), output_specifics.name());
}

}  // namespace
