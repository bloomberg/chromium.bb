// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/snapshotting_command_storage_backend.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MakeRefCounted;

namespace sessions {
namespace {

using SessionCommands = std::vector<std::unique_ptr<sessions::SessionCommand>>;

struct TestData {
  sessions::SessionCommand::id_type command_id;
  std::string data;
};

std::unique_ptr<sessions::SessionCommand> CreateCommandFromData(
    const TestData& data) {
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(
          data.command_id,
          static_cast<sessions::SessionCommand::size_type>(data.data.size()));
  if (!data.data.empty())
    memcpy(command->contents(), data.data.c_str(), data.data.size());
  return command;
}

bool IsCanceled() {
  return false;
}

}  // namespace

class SnapshottingCommandStorageBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("SessionTestDirs"));
    base::CreateDirectory(path_);
  }

  void AssertCommandEqualsData(const TestData& data,
                               sessions::SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_TRUE(
        memcmp(command->contents(), data.data.c_str(), command->size()) == 0);
  }

  scoped_refptr<SnapshottingCommandStorageBackend> CreateBackend() {
    return MakeRefCounted<SnapshottingCommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(),
        sessions::SnapshottingCommandStorageManager::SESSION_RESTORE, path_);
  }

  void ReadLastSessionCommands(
      SnapshottingCommandStorageBackend* backend,
      std::vector<std::unique_ptr<SessionCommand>>* commands) {
    backend->ReadLastSessionCommands(
        base::BindRepeating(&IsCanceled),
        base::BindLambdaForTesting(
            [&commands](std::vector<std::unique_ptr<SessionCommand>> result) {
              *commands = std::move(result);
            }));
  }

  base::test::TaskEnvironment task_environment_;
  // Path used in testing.
  base::FilePath path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SnapshottingCommandStorageBackendTest, SimpleReadWrite) {
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false);
  ASSERT_TRUE(commands.empty());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  ReadLastSessionCommands(backend.get(), &commands);

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  commands.clear();

  backend = nullptr;
  backend = CreateBackend();
  ReadLastSessionCommands(backend.get(), &commands);

  ASSERT_EQ(0U, commands.size());

  // Make sure we can delete.
  backend->DeleteLastSession();
  ReadLastSessionCommands(backend.get(), &commands);
  ASSERT_EQ(0U, commands.size());
}

TEST_F(SnapshottingCommandStorageBackendTest, RandomData) {
  struct TestData data[] = {
      {1, "a"},
      {2, "ab"},
      {3, "abc"},
      {4, "abcd"},
      {5, "abcde"},
      {6, "abcdef"},
      {7, "abcdefg"},
      {8, "abcdefgh"},
      {9, "abcdefghi"},
      {10, "abcdefghij"},
      {11, "abcdefghijk"},
      {12, "abcdefghijkl"},
      {13, "abcdefghijklm"},
  };

  for (size_t i = 0; i < base::size(data); ++i) {
    scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
    SessionCommands commands;
    if (i != 0) {
      // Read previous data.
      ReadLastSessionCommands(backend.get(), &commands);
      ASSERT_EQ(i, commands.size());
      for (auto j = commands.begin(); j != commands.end(); ++j)
        AssertCommandEqualsData(data[j - commands.begin()], j->get());

      backend->AppendCommands(std::move(commands), false);
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(std::move(commands), false);
  }
}

TEST_F(SnapshottingCommandStorageBackendTest, BigData) {
  struct TestData data[] = {
      {1, "a"},
      {2, "ab"},
  };

  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;

  commands.push_back(CreateCommandFromData(data[0]));
  const sessions::SessionCommand::size_type big_size =
      SnapshottingCommandStorageBackend::kFileReadBufferSize + 100;
  const sessions::SessionCommand::id_type big_id = 50;
  std::unique_ptr<sessions::SessionCommand> big_command =
      std::make_unique<sessions::SessionCommand>(big_id, big_size);
  reinterpret_cast<char*>(big_command->contents())[0] = 'a';
  reinterpret_cast<char*>(big_command->contents())[big_size - 1] = 'z';
  commands.push_back(std::move(big_command));
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(std::move(commands), false);

  backend = nullptr;
  backend = CreateBackend();

  ReadLastSessionCommands(backend.get(), &commands);
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[2].get());

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
  commands.clear();
}

TEST_F(SnapshottingCommandStorageBackendTest, EmptyCommand) {
  TestData empty_command;
  empty_command.command_id = 1;
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  SessionCommands empty_commands;
  empty_commands.push_back(CreateCommandFromData(empty_command));
  backend->AppendCommands(std::move(empty_commands), true);
  backend->MoveCurrentSessionToLastSession();

  SessionCommands commands;
  ReadLastSessionCommands(backend.get(), &commands);
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(empty_command, commands[0].get());
  commands.clear();
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_F(SnapshottingCommandStorageBackendTest, Truncate) {
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), false);

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(std::move(commands), true);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  ReadLastSessionCommands(backend.get(), &commands);

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());

  commands.clear();
}

}  // namespace sessions
