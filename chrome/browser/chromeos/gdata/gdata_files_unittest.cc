// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <string>
#include <utility>
#include <vector>
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

TEST(GDataRootDirectoryTest, ParseFromString_DetectBadTitle) {
  GDataRootDirectoryProto proto;
  GDataEntryProto* mutable_entry =
      proto.mutable_gdata_directory()->mutable_gdata_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_resource_id(kGDataRootDirectoryResourceId);

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  GDataRootDirectory root;
  // This should fail as the title is empty.
  // root.title() should be unchanged.
  ASSERT_FALSE(root.ParseFromString(serialized_proto));
  ASSERT_EQ(kGDataRootDirectory, root.title());

  // Setting the title to "gdata".
  mutable_entry->set_title("gdata");
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  // This should fail as the title is not kGDataRootDirectory.
  // root.title() should be unchanged.
  ASSERT_FALSE(root.ParseFromString(serialized_proto));
  ASSERT_EQ(kGDataRootDirectory, root.title());

  // Setting the title to kGDataRootDirectory.
  mutable_entry->set_title(kGDataRootDirectory);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  // This should succeed as the title is kGDataRootDirectory.
  ASSERT_TRUE(root.ParseFromString(serialized_proto));
  ASSERT_EQ(kGDataRootDirectory, root.title());
}

TEST(GDataRootDirectoryTest, ParseFromString_DetectBadResourceID) {
  GDataRootDirectoryProto proto;
  GDataEntryProto* mutable_entry =
      proto.mutable_gdata_directory()->mutable_gdata_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_title(kGDataRootDirectory);

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  GDataRootDirectory root;
  // This should fail as the resource ID is empty.
  // root.resource_id() should be unchanged.
  ASSERT_FALSE(root.ParseFromString(serialized_proto));
  EXPECT_EQ(kGDataRootDirectoryResourceId, root.resource_id());

  // Set the correct resource ID.
  mutable_entry->set_resource_id(kGDataRootDirectoryResourceId);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  // This should succeed as the resource ID is correct.
  ASSERT_TRUE(root.ParseFromString(serialized_proto));
  EXPECT_EQ(kGDataRootDirectoryResourceId, root.resource_id());
}

TEST(GDataRootDirectoryTest, GetEntryByResourceId_RootDirectory) {
  GDataRootDirectory root;
  // Look up the root directory by its resource ID.
  GDataEntry* entry = root.GetEntryByResourceId(kGDataRootDirectoryResourceId);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->AsGDataRootDirectory());
  EXPECT_EQ(&root, entry->AsGDataRootDirectory());
}

}  // namespace gdata
