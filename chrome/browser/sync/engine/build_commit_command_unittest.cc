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
  EXPECT_LT(BuildCommitCommand::kFirstPosition,
            BuildCommitCommand::kLastPosition);

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
  EXPECT_EQ(0x32535ffe3dc97LL + BuildCommitCommand::kGap,
      command_.InterpolatePosition(0x32535ffe3dc97LL, 0x61abcd323122cLL));
  EXPECT_EQ(~0x61abcd323122cLL + BuildCommitCommand::kGap,
      command_.InterpolatePosition(~0x61abcd323122cLL, ~0x32535ffe3dc97LL));

  // Lower limits
  EXPECT_EQ(BuildCommitCommand::kFirstPosition + 0x20,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition,
                                   BuildCommitCommand::kFirstPosition + 0x100));
  EXPECT_EQ(BuildCommitCommand::kFirstPosition + 2,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition + 1,
                                   BuildCommitCommand::kFirstPosition + 2));
  EXPECT_EQ(BuildCommitCommand::kFirstPosition + BuildCommitCommand::kGap/8 + 1,
      command_.InterpolatePosition(
          BuildCommitCommand::kFirstPosition + 1,
          BuildCommitCommand::kFirstPosition + 1 + BuildCommitCommand::kGap));

  // Extremal cases.
  EXPECT_EQ(0,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition,
                                   BuildCommitCommand::kLastPosition));
  EXPECT_EQ(BuildCommitCommand::kFirstPosition + 1 + BuildCommitCommand::kGap,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition + 1,
                                   BuildCommitCommand::kLastPosition));
  EXPECT_EQ(BuildCommitCommand::kFirstPosition + 1 + BuildCommitCommand::kGap,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition + 1,
                                   BuildCommitCommand::kLastPosition - 1));
  EXPECT_EQ(BuildCommitCommand::kLastPosition - 1 - BuildCommitCommand::kGap,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition,
                                   BuildCommitCommand::kLastPosition - 1));

  // Edge cases around zero.
  EXPECT_EQ(BuildCommitCommand::kGap,
      command_.InterpolatePosition(0, BuildCommitCommand::kLastPosition));
  EXPECT_EQ(BuildCommitCommand::kGap + 1,
      command_.InterpolatePosition(1, BuildCommitCommand::kLastPosition));
  EXPECT_EQ(BuildCommitCommand::kGap - 1,
      command_.InterpolatePosition(-1, BuildCommitCommand::kLastPosition));
  EXPECT_EQ(-BuildCommitCommand::kGap,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition, 0));
  EXPECT_EQ(-BuildCommitCommand::kGap + 1,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition, 1));
  EXPECT_EQ(-BuildCommitCommand::kGap - 1,
      command_.InterpolatePosition(BuildCommitCommand::kFirstPosition, -1));
  EXPECT_EQ(BuildCommitCommand::kGap / 8,
      command_.InterpolatePosition(0, BuildCommitCommand::kGap));
  EXPECT_EQ(BuildCommitCommand::kGap / 4,
      command_.InterpolatePosition(0, BuildCommitCommand::kGap*2));
  EXPECT_EQ(BuildCommitCommand::kGap,
      command_.InterpolatePosition(0, BuildCommitCommand::kGap*2 + 1));
}

}  // namespace browser_sync


