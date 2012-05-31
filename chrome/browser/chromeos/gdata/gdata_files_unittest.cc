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

TEST(GDataRootDirectoryTest, RefreshFile) {
  GDataRootDirectory root;
  // Add a directory to the file system.
  GDataDirectory* directory_entry = new GDataDirectory(&root, &root);
  directory_entry->set_resource_id("folder:directory_resource_id");
  root.AddEntry(directory_entry);

  // Add a new file to the directory.
  GDataFile* initial_file_entry = new GDataFile(NULL, &root);
  initial_file_entry->set_resource_id("file:file_resource_id");
  directory_entry->AddEntry(initial_file_entry);
  ASSERT_EQ(directory_entry, initial_file_entry->parent());

  // Initial file system state set, let's try refreshing entries.

  // New value for the entry with resource id "file:file_resource_id".
  GDataFile* new_file_entry = new GDataFile(NULL, &root);
  new_file_entry->set_resource_id("file:file_resource_id");
  root.RefreshFile(scoped_ptr<GDataFile>(new_file_entry).Pass());
  // Root should have |new_file_entry|, not |initial_file_entry|.
  // If this is not true, |new_file_entry| has probably been destroyed, hence
  // ASSERT (we're trying to access |new_file_entry| later on).
  ASSERT_EQ(new_file_entry, root.GetEntryByResourceId("file:file_resource_id"));
  // We have just verified new_file_entry exists inside root, so accessing
  // |new_file_entry->parent()| should be safe.
  EXPECT_EQ(directory_entry, new_file_entry->parent());

  // Let's try refreshing file that didn't prviously exist.
  GDataFile* non_existent_entry = new GDataFile(NULL, &root);
  non_existent_entry->set_resource_id("file:does_not_exist");
  root.RefreshFile(scoped_ptr<GDataFile>(non_existent_entry).Pass());
  // File with non existent resource id should not be added.
  EXPECT_FALSE(root.GetEntryByResourceId("file:does_not_exist"));
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
