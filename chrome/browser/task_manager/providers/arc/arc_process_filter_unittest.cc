// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/arc/arc_process_filter.h"

#include <string>

#include "chrome/browser/chromeos/arc/process/arc_process.h"
#include "components/arc/common/process.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

const arc::ArcProcess kTest1Process(0 /* nspid */,
                                    0 /* pid */,
                                    "/system/test1_process",
                                    arc::mojom::ProcessState::PERSISTENT,
                                    false /* is_focused */,
                                    0 /* last_activity_time */);

const arc::ArcProcess kTest2Process(0 /* nspid */,
                                    0 /* pid */,
                                    "/system/test2_process",
                                    arc::mojom::ProcessState::PERSISTENT,
                                    false /* is_focused */,
                                    0 /* last_activity_time */);

const arc::ArcProcess kTopProcess(0 /* nspid */,
                                  0 /* pid */,
                                  "top_process",
                                  arc::mojom::ProcessState::TOP,
                                  false /* is_focused */,
                                  0 /* last_activity_time */);

}  // namespace

using ArcProcessFilterTest = testing::Test;

TEST_F(ArcProcessFilterTest, ExplicitWhitelist) {
  ArcProcessFilter filter({"/system/test1_process", "test"});
  EXPECT_TRUE(filter.ShouldDisplayProcess(kTest1Process));
  EXPECT_FALSE(filter.ShouldDisplayProcess(kTest2Process));
}

TEST_F(ArcProcessFilterTest, ProcessStateFiltering) {
  ArcProcessFilter filter;
  EXPECT_FALSE(filter.ShouldDisplayProcess(kTest1Process));
  EXPECT_FALSE(filter.ShouldDisplayProcess(kTest2Process));
  EXPECT_TRUE(filter.ShouldDisplayProcess(kTopProcess));
}

}  // namespace task_manager
