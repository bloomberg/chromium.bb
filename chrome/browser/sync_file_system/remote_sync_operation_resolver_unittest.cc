// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <vector>

#include "chrome/browser/sync_file_system/remote_sync_operation_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_file_type.h"

namespace sync_file_system {

namespace {

struct Input {
  SyncFileType local_file_type;
  FileChangeList local_changes;

  std::string DebugString() {
    std::ostringstream ss;
    ss << "LocalFileType: " << local_file_type
       << ", LocalChanges: " << local_changes.DebugString();
    return ss.str();
  }
};

template <typename type, size_t array_size>
std::vector<type> CreateList(const type (&inputs)[array_size]) {
  return std::vector<type>(inputs, inputs + array_size);
}

FileChangeList CreateEmptyChange() {
  return FileChangeList();
}

FileChangeList CreateAddOrUpdateFileChange() {
  FileChangeList list;
  list.Update(FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                         SYNC_FILE_TYPE_FILE));
  return list;
}

FileChangeList CreateAddDirectoryChange() {
  FileChangeList list;
  list.Update(FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                         SYNC_FILE_TYPE_DIRECTORY));
  return list;
}

FileChangeList CreateDeleteFileChange() {
  FileChangeList list;
  list.Update(FileChange(FileChange::FILE_CHANGE_DELETE,
                         SYNC_FILE_TYPE_FILE));
  return list;
}

FileChangeList CreateDeleteDirectoryChange() {
  FileChangeList list;
  list.Update(FileChange(FileChange::FILE_CHANGE_DELETE,
                         SYNC_FILE_TYPE_DIRECTORY));
  return list;
}

std::vector<Input> CreateInput() {
  const Input inputs[] = {
    { SYNC_FILE_TYPE_UNKNOWN, CreateEmptyChange() },
    { SYNC_FILE_TYPE_UNKNOWN, CreateAddOrUpdateFileChange() },
    { SYNC_FILE_TYPE_UNKNOWN, CreateAddDirectoryChange() },
    { SYNC_FILE_TYPE_UNKNOWN, CreateDeleteFileChange() },
    { SYNC_FILE_TYPE_UNKNOWN, CreateDeleteDirectoryChange() },

    { SYNC_FILE_TYPE_FILE, CreateEmptyChange() },
    { SYNC_FILE_TYPE_FILE, CreateAddOrUpdateFileChange() },
    { SYNC_FILE_TYPE_FILE, CreateAddDirectoryChange() },
    { SYNC_FILE_TYPE_FILE, CreateDeleteFileChange() },
    { SYNC_FILE_TYPE_FILE, CreateDeleteDirectoryChange() },

    { SYNC_FILE_TYPE_DIRECTORY, CreateEmptyChange() },
    { SYNC_FILE_TYPE_DIRECTORY, CreateAddOrUpdateFileChange() },
    { SYNC_FILE_TYPE_DIRECTORY, CreateAddDirectoryChange() },
    { SYNC_FILE_TYPE_DIRECTORY, CreateDeleteFileChange() },
    { SYNC_FILE_TYPE_DIRECTORY, CreateDeleteDirectoryChange() },
  };
  return CreateList(inputs);
}

}  // namespace

class RemoteSyncOperationResolverTest : public testing::Test {
 public:
  RemoteSyncOperationResolverTest() {}

 protected:
  typedef RemoteSyncOperationResolver Resolver;
  typedef std::vector<RemoteSyncOperationType> ExpectedTypes;

  DISALLOW_COPY_AND_ASSIGN(RemoteSyncOperationResolverTest);
};

TEST_F(RemoteSyncOperationResolverTest, ResolveForAddOrUpdateFile) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_ADD_FILE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_ADD_FILE,
    REMOTE_SYNC_OPERATION_ADD_FILE,

    REMOTE_SYNC_OPERATION_UPDATE_FILE,
    REMOTE_SYNC_OPERATION_CONFLICT,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddOrUpdateFile(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ":  (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForAddOrUpdateFileInConflict) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,

    REMOTE_SYNC_OPERATION_CONFLICT,
    REMOTE_SYNC_OPERATION_CONFLICT,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddOrUpdateFileInConflict(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForAddDirectory) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_ADD_DIRECTORY,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,

    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddDirectory(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForAddDirectoryInConflict) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,

    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddDirectoryInConflict(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForDeleteFile) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_DELETE_METADATA,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_DELETE_METADATA,
    REMOTE_SYNC_OPERATION_DELETE_METADATA,

    REMOTE_SYNC_OPERATION_DELETE_FILE,
    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < inputs.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteFile(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForDeleteFileInConflict) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_DELETE_METADATA,
    REMOTE_SYNC_OPERATION_DELETE_METADATA,

    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteFileInConflict(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForDeleteDirectory) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_NONE,
    REMOTE_SYNC_OPERATION_NONE,

    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_DELETE_DIRECTORY,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteDirectory(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(RemoteSyncOperationResolverTest, ResolveForDeleteDirectoryInConflict) {
  const RemoteSyncOperationType kExpectedTypes[] = {
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_DELETE_METADATA,
    REMOTE_SYNC_OPERATION_DELETE_METADATA,

    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,

    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    REMOTE_SYNC_OPERATION_FAIL,
    REMOTE_SYNC_OPERATION_FAIL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteDirectoryInConflict(
                  inputs[i].local_changes, inputs[i].local_file_type))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

}  // namespace sync_file_system
