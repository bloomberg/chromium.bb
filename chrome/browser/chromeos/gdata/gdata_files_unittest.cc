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

namespace {

// See gdata.proto for the difference between the two URLs.
const char kResumableEditMediaUrl[] = "http://resumable-edit-media/";
const char kResumableCreateMediaUrl[] = "http://resumable-create-media/";

}  // namespace

TEST(GDataEntryTest, FromProto_DetectBadUploadUrl) {
  GDataEntryProto proto;
  proto.set_title("test.txt");

  GDataEntry entry(NULL, NULL);
  // This should fail as the upload URL is empty.
  ASSERT_FALSE(entry.FromProto(proto));

  // Set a upload URL.
  proto.set_upload_url(kResumableEditMediaUrl);

  // This should succeed as the upload URL is set.
  ASSERT_TRUE(entry.FromProto(proto));
  EXPECT_EQ(kResumableEditMediaUrl, entry.upload_url().spec());
}

TEST(GDataRootDirectoryTest, ParseFromString_DetectBadTitle) {
  GDataRootDirectoryProto proto;
  GDataEntryProto* mutable_entry =
      proto.mutable_gdata_directory()->mutable_gdata_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_resource_id(kGDataRootDirectoryResourceId);
  mutable_entry->set_upload_url(kResumableCreateMediaUrl);

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  GDataDirectoryService directory_service;
  GDataDirectory* root(directory_service.root());
  // This should fail as the title is empty.
  // root.title() should be unchanged.
  ASSERT_FALSE(directory_service.ParseFromString(serialized_proto));
  ASSERT_EQ(kGDataRootDirectory, root->title());

  // Setting the title to "gdata".
  mutable_entry->set_title("gdata");
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  // This should fail as the title is not kGDataRootDirectory.
  // root.title() should be unchanged.
  ASSERT_FALSE(directory_service.ParseFromString(serialized_proto));
  ASSERT_EQ(kGDataRootDirectory, root->title());

  // Setting the title to kGDataRootDirectory.
  mutable_entry->set_title(kGDataRootDirectory);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  // This should succeed as the title is kGDataRootDirectory.
  ASSERT_TRUE(directory_service.ParseFromString(serialized_proto));
  ASSERT_EQ(kGDataRootDirectory, root->title());
}

TEST(GDataRootDirectoryTest, ParseFromString_DetectBadResourceID) {
  GDataRootDirectoryProto proto;
  GDataEntryProto* mutable_entry =
      proto.mutable_gdata_directory()->mutable_gdata_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_title(kGDataRootDirectory);
  mutable_entry->set_upload_url(kResumableCreateMediaUrl);

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  GDataDirectoryService directory_service;
  GDataDirectory* root(directory_service.root());
  // This should fail as the resource ID is empty.
  // root.resource_id() should be unchanged.
  ASSERT_FALSE(directory_service.ParseFromString(serialized_proto));
  EXPECT_EQ(kGDataRootDirectoryResourceId, root->resource_id());

  // Set the correct resource ID.
  mutable_entry->set_resource_id(kGDataRootDirectoryResourceId);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));

  // This should succeed as the resource ID is correct.
  ASSERT_TRUE(directory_service.ParseFromString(serialized_proto));
  EXPECT_EQ(kGDataRootDirectoryResourceId, root->resource_id());
}

// We have a similar test in FromProto_DetectBadUploadUrl, but the test here
// is to ensure that an error in GDataFile::FromProto() is properly
// propagated to GDataRootDirectory::ParseFromString().
TEST(GDataRootDirectoryTest, ParseFromString_DetectNoUploadUrl) {
  // Set up the root directory properly.
  GDataRootDirectoryProto root_directory_proto;
  GDataEntryProto* mutable_entry =
      root_directory_proto.mutable_gdata_directory()->mutable_gdata_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_title(kGDataRootDirectory);
  mutable_entry->set_resource_id(kGDataRootDirectoryResourceId);
  mutable_entry->set_upload_url(kResumableCreateMediaUrl);
  // Set the origin to the server.
  root_directory_proto.mutable_gdata_directory()->set_origin(FROM_SERVER);

  // Add an empty sub directory under the root directory. This directory is
  // added to ensure that nothing is left when the parsing failed.
  GDataDirectoryProto* sub_directory_proto =
      root_directory_proto.mutable_gdata_directory()->add_child_directories();
  sub_directory_proto->mutable_gdata_entry()->mutable_file_info()->
      set_is_directory(true);
  sub_directory_proto->mutable_gdata_entry()->set_title("empty");
  sub_directory_proto->mutable_gdata_entry()->set_upload_url(
      kResumableCreateMediaUrl);

  // Add a sub directory under the root directory.
  sub_directory_proto =
      root_directory_proto.mutable_gdata_directory()->add_child_directories();
  sub_directory_proto->mutable_gdata_entry()->mutable_file_info()->
      set_is_directory(true);
  sub_directory_proto->mutable_gdata_entry()->set_title("dir");
  sub_directory_proto->mutable_gdata_entry()->set_upload_url(
      kResumableCreateMediaUrl);

  // Add a new file under the sub directory "dir".
  GDataFileProto* file_proto =
      sub_directory_proto->add_child_files();
  file_proto->mutable_gdata_entry()->set_title("test.txt");

  GDataDirectoryService directory_service;
  GDataDirectory* root(directory_service.root());
  // The origin is set to UNINITIALIZED by default.
  ASSERT_EQ(UNINITIALIZED, root->origin());
  std::string serialized_proto;
  // Serialize the proto and check if it's loaded.
  // This should fail as the upload URL is not set for |file_proto|.
  ASSERT_TRUE(root_directory_proto.SerializeToString(&serialized_proto));
  ASSERT_FALSE(directory_service.ParseFromString(serialized_proto));
  // Nothing should be added to the root directory if the parse failed.
  ASSERT_TRUE(root->child_files().empty());
  ASSERT_TRUE(root->child_directories().empty());
  // The origin should remain UNINITIALIZED because the loading failed.
  ASSERT_EQ(UNINITIALIZED, root->origin());

  // Set an upload URL.
  file_proto->mutable_gdata_entry()->set_upload_url(kResumableEditMediaUrl);

  // Serialize the proto and check if it's loaded.
  // This should succeed as the upload URL is set for |file_proto|.
  ASSERT_TRUE(root_directory_proto.SerializeToString(&serialized_proto));
  ASSERT_TRUE(directory_service.ParseFromString(serialized_proto));
  // No file should be added to the root directory.
  ASSERT_TRUE(root->child_files().empty());
  // Two directories ("empty", "dir") should be added to the root directory.
  ASSERT_EQ(2U, root->child_directories().size());
  // The origin should change to FROM_CACHE because we loaded from the cache.
  ASSERT_EQ(FROM_CACHE, root->origin());
}

TEST(GDataRootDirectoryTest, RefreshFile) {
  GDataDirectoryService directory_service;
  GDataDirectory* root(directory_service.root());
  // Add a directory to the file system.
  GDataDirectory* directory_entry = new GDataDirectory(root,
                                                       &directory_service);
  directory_entry->set_resource_id("folder:directory_resource_id");
  root->AddEntry(directory_entry);

  // Add a new file to the directory.
  GDataFile* initial_file_entry = new GDataFile(NULL, &directory_service);
  initial_file_entry->set_resource_id("file:file_resource_id");
  directory_entry->AddEntry(initial_file_entry);
  ASSERT_EQ(directory_entry, initial_file_entry->parent());

  // Initial file system state set, let's try refreshing entries.

  // New value for the entry with resource id "file:file_resource_id".
  GDataFile* new_file_entry = new GDataFile(NULL, &directory_service);
  new_file_entry->set_resource_id("file:file_resource_id");
  directory_service.RefreshFile(scoped_ptr<GDataFile>(new_file_entry).Pass());
  // Root should have |new_file_entry|, not |initial_file_entry|.
  // If this is not true, |new_file_entry| has probably been destroyed, hence
  // ASSERT (we're trying to access |new_file_entry| later on).
  ASSERT_EQ(new_file_entry,
      directory_service.GetEntryByResourceId("file:file_resource_id"));
  // We have just verified new_file_entry exists inside root, so accessing
  // |new_file_entry->parent()| should be safe.
  EXPECT_EQ(directory_entry, new_file_entry->parent());

  // Let's try refreshing file that didn't prviously exist.
  GDataFile* non_existent_entry = new GDataFile(NULL, &directory_service);
  non_existent_entry->set_resource_id("file:does_not_exist");
  directory_service.RefreshFile(
      scoped_ptr<GDataFile>(non_existent_entry).Pass());
  // File with non existent resource id should not be added.
  EXPECT_FALSE(directory_service.GetEntryByResourceId("file:does_not_exist"));
}

TEST(GDataRootDirectoryTest, GetEntryByResourceId_RootDirectory) {
  GDataDirectoryService directory_service;
  // Look up the root directory by its resource ID.
  GDataEntry* entry = directory_service.GetEntryByResourceId(
      kGDataRootDirectoryResourceId);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kGDataRootDirectoryResourceId, entry->resource_id());
}

}  // namespace gdata
