// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/sync_file_system/local_sync_operation_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_file_type.h"

namespace sync_file_system {

namespace {

struct Input {
  bool has_remote_change;
  FileChange remote_file_change;

  std::string DebugString() {
    std::ostringstream ss;
    ss << "has_remote_change: " << (has_remote_change ? "true" : "false")
       << ", RemoteFileChange: " << remote_file_change.DebugString();
    return ss.str();
  }
};

template <typename type, size_t array_size>
std::vector<type> CreateList(const type (&inputs)[array_size]) {
  return std::vector<type>(inputs, inputs + array_size);
}

FileChange CreateDummyFileChange() {
  return FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                    SYNC_FILE_TYPE_UNKNOWN);
}

std::vector<Input> CreateInput() {
  const Input inputs[] = {
    { false, CreateDummyFileChange() },
    { true, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE) },
    { true, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY) },
    { true, FileChange(FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_FILE) },
    { true, FileChange(FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_DIRECTORY) },
  };
  return CreateList(inputs);
}

}  // namespace

class LocalSyncOperationResolverTest : public testing::Test {
 public:
  LocalSyncOperationResolverTest() {}

 protected:
  typedef LocalSyncOperationResolver Resolver;
  typedef std::vector<LocalSyncOperationType> ExpectedTypes;

  DISALLOW_COPY_AND_ASSIGN(LocalSyncOperationResolverTest);
};

TEST_F(LocalSyncOperationResolverTest, ResolveForAddOrUpdateFile) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_ADD_FILE,
    LOCAL_SYNC_OPERATION_CONFLICT,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  // TODO(nhiroki): Fix inputs so that these tests can cover all cases.
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddOrUpdateFile(
                  inputs[i].has_remote_change, inputs[i].remote_file_change,
                  SYNC_FILE_TYPE_UNKNOWN))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForAddOrUpdateFileInConflict) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_CONFLICT,
    LOCAL_SYNC_OPERATION_CONFLICT,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddOrUpdateFileInConflict(
                  inputs[i].has_remote_change, inputs[i].remote_file_change))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForAddDirectory) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_ADD_DIRECTORY,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_NONE,
    LOCAL_SYNC_OPERATION_ADD_DIRECTORY,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  // TODO(nhiroki): Fix inputs so that these tests can cover all cases.
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddDirectory(
                  inputs[i].has_remote_change, inputs[i].remote_file_change,
                  SYNC_FILE_TYPE_UNKNOWN))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForAddDirectoryInConflict) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForAddDirectoryInConflict(
                  inputs[i].has_remote_change, inputs[i].remote_file_change))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForDeleteFile) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_NONE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  // TODO(nhiroki): Fix inputs so that these tests can cover all cases.
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteFile(
                  inputs[i].has_remote_change, inputs[i].remote_file_change,
                  SYNC_FILE_TYPE_UNKNOWN))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForDeleteFileInConflict) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteFileInConflict(
                  inputs[i].has_remote_change, inputs[i].remote_file_change))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForDeleteDirectory) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_NONE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  // TODO(nhiroki): Fix inputs so that these tests can cover all cases.
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteDirectory(
                  inputs[i].has_remote_change, inputs[i].remote_file_change,
                  SYNC_FILE_TYPE_UNKNOWN))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

TEST_F(LocalSyncOperationResolverTest, ResolveForDeleteDirectoryInConflict) {
  const LocalSyncOperationType kExpectedTypes[] = {
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
    LOCAL_SYNC_OPERATION_DELETE_METADATA,
  };

  ExpectedTypes expected_types = CreateList(kExpectedTypes);
  std::vector<Input> inputs = CreateInput();

  ASSERT_EQ(expected_types.size(), inputs.size());
  for (ExpectedTypes::size_type i = 0; i < expected_types.size(); ++i)
    EXPECT_EQ(expected_types[i],
              Resolver::ResolveForDeleteDirectoryInConflict(
                  inputs[i].has_remote_change, inputs[i].remote_file_change))
        << "Case " << i << ": (" << inputs[i].DebugString() << ")";
}

}  // namespace sync_file_system
