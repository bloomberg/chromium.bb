// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/build_commit_command.h"
#include "chrome/test/sync/engine/syncer_command_test.h"

namespace browser_sync {

// A test fixture for tests exercising ClearDataCommandTest.
class BuildCommitCommandTest : public SyncerCommandTest {
 protected:
  BuildCommitCommandTest() {}
  BuildCommitCommand command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BuildCommitCommandTest);
};

TEST_F(BuildCommitCommandTest, InterpolatePosition) {
  EXPECT_LT(BuildCommitCommand::GetFirstPosition(),
            BuildCommitCommand::GetLastPosition());

  // Dense ranges.
  EXPECT_EQ(10, command_.InterpolatePosition(10, 10));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 11));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 12));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 13));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 14));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 15));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 16));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 17));
  EXPECT_EQ(11, command_.InterpolatePosition(10, 18));
  EXPECT_EQ(12, command_.InterpolatePosition(10, 19));
  EXPECT_EQ(12, command_.InterpolatePosition(10, 20));

  // Sparse ranges.
  EXPECT_EQ(0x32535ffe3dc97LL + BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(0x32535ffe3dc97LL, 0x61abcd323122cLL));
  EXPECT_EQ(~0x61abcd323122cLL + BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(~0x61abcd323122cLL, ~0x32535ffe3dc97LL));

  // Lower limits
  EXPECT_EQ(BuildCommitCommand::GetFirstPosition() + 0x20,
      command_.InterpolatePosition(
          BuildCommitCommand::GetFirstPosition(),
          BuildCommitCommand::GetFirstPosition() + 0x100));
  EXPECT_EQ(BuildCommitCommand::GetFirstPosition() + 2,
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition() + 1,
                                   BuildCommitCommand::GetFirstPosition() + 2));
  EXPECT_EQ(BuildCommitCommand::GetFirstPosition() +
            BuildCommitCommand::GetGap()/8 + 1,
      command_.InterpolatePosition(
          BuildCommitCommand::GetFirstPosition() + 1,
          BuildCommitCommand::GetFirstPosition() + 1 +
          BuildCommitCommand::GetGap()));

  // Extremal cases.
  EXPECT_EQ(0,
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition(),
                                   BuildCommitCommand::GetLastPosition()));
  EXPECT_EQ(BuildCommitCommand::GetFirstPosition() + 1 +
            BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition() + 1,
                                   BuildCommitCommand::GetLastPosition()));
  EXPECT_EQ(BuildCommitCommand::GetFirstPosition() + 1 +
            BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition() + 1,
                                   BuildCommitCommand::GetLastPosition() - 1));
  EXPECT_EQ(BuildCommitCommand::GetLastPosition() - 1 -
            BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition(),
                                   BuildCommitCommand::GetLastPosition() - 1));

  // Edge cases around zero.
  EXPECT_EQ(BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(0, BuildCommitCommand::GetLastPosition()));
  EXPECT_EQ(BuildCommitCommand::GetGap() + 1,
      command_.InterpolatePosition(1, BuildCommitCommand::GetLastPosition()));
  EXPECT_EQ(BuildCommitCommand::GetGap() - 1,
      command_.InterpolatePosition(-1, BuildCommitCommand::GetLastPosition()));
  EXPECT_EQ(-BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition(), 0));
  EXPECT_EQ(-BuildCommitCommand::GetGap() + 1,
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition(), 1));
  EXPECT_EQ(-BuildCommitCommand::GetGap() - 1,
      command_.InterpolatePosition(BuildCommitCommand::GetFirstPosition(), -1));
  EXPECT_EQ(BuildCommitCommand::GetGap() / 8,
      command_.InterpolatePosition(0, BuildCommitCommand::GetGap()));
  EXPECT_EQ(BuildCommitCommand::GetGap() / 4,
      command_.InterpolatePosition(0, BuildCommitCommand::GetGap()*2));
  EXPECT_EQ(BuildCommitCommand::GetGap(),
      command_.InterpolatePosition(0, BuildCommitCommand::GetGap()*2 + 1));
}

}  // namespace browser_sync


