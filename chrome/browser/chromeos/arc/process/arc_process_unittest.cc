// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/process/arc_process.h"

#include <assert.h>

#include <list>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Tests that ArcProcess objects can be sorted by their priority (higher to
// lower). This is critical for the OOM handler to work correctly.
TEST(ArcProcess, TestSorting) {
  constexpr int64_t kNow = 1234567890;

  std::list<ArcProcess> processes;  // use list<> for emplace_front.
  processes.emplace_back(0, 0, "process 0",
                         mojom::ProcessStateDeprecated::PERSISTENT,
                         false /* is_focused */, kNow + 1);
  processes.emplace_front(1, 1, "process 1",
                          mojom::ProcessStateDeprecated::PERSISTENT, false,
                          kNow);
  processes.emplace_back(2, 2, "process 2",
                         mojom::ProcessStateDeprecated::LAST_ACTIVITY, false,
                         kNow);
  processes.emplace_front(3, 3, "process 3",
                          mojom::ProcessStateDeprecated::LAST_ACTIVITY, false,
                          kNow + 1);
  processes.emplace_back(4, 4, "process 4",
                         mojom::ProcessStateDeprecated::CACHED_EMPTY, false,
                         kNow + 1);
  processes.emplace_front(5, 5, "process 5",
                          mojom::ProcessStateDeprecated::CACHED_EMPTY, false,
                          kNow);
  processes.sort();

  static_assert(mojom::ProcessStateDeprecated::PERSISTENT <
                    mojom::ProcessStateDeprecated::LAST_ACTIVITY,
                "unexpected enum values");
  static_assert(mojom::ProcessStateDeprecated::LAST_ACTIVITY <
                    mojom::ProcessStateDeprecated::CACHED_EMPTY,
                "unexpected enum values");

  std::list<ArcProcess>::const_iterator it = processes.begin();
  // 0 should have higher priority since its last_activity_time is more recent.
  EXPECT_EQ(0, it->pid());
  ++it;
  EXPECT_EQ(1, it->pid());
  ++it;
  // Same, 3 should have higher priority.
  EXPECT_EQ(3, it->pid());
  ++it;
  EXPECT_EQ(2, it->pid());
  ++it;
  // Same, 4 should have higher priority.
  EXPECT_EQ(4, it->pid());
  ++it;
  EXPECT_EQ(5, it->pid());
}

TEST(ArcProcess, TestIsImportant) {
  constexpr bool kIsNotFocused = false;

  // Processes up to IMPORTANT_FOREGROUND are considered important.
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::PERSISTENT,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::PERSISTENT_UI,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(
      ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::TOP, true, 0)
          .IsImportant());
  EXPECT_TRUE(
      ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::TOP, false, 0)
          .IsImportant());
  EXPECT_TRUE(
      ArcProcess(0, 0, "process",
                 mojom::ProcessStateDeprecated::BOUND_FOREGROUND_SERVICE,
                 kIsNotFocused, 0)
          .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::FOREGROUND_SERVICE,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::TOP_SLEEPING,
                         kIsNotFocused, 0)
                  .IsImportant());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::IMPORTANT_FOREGROUND,
                         kIsNotFocused, 0)
                  .IsImportant());

  // Others are not important.
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::IMPORTANT_BACKGROUND,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::BACKUP, kIsNotFocused,
                          0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::HEAVY_WEIGHT,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::SERVICE, kIsNotFocused,
                          0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::RECEIVER,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::HOME,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::LAST_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::CACHED_ACTIVITY,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::CACHED_ACTIVITY_CLIENT,
                          kIsNotFocused, 0)
                   .IsImportant());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::CACHED_EMPTY,
                          kIsNotFocused, 0)
                   .IsImportant());

  // Custom ARC protected processes.
  EXPECT_TRUE(ArcProcess(0, 0, "com.google.android.apps.work.clouddpc.arc",
                         mojom::ProcessStateDeprecated::SERVICE, kIsNotFocused,
                         0)
                  .IsImportant());
}

TEST(ArcProcess, TestIsKernelKillable) {
  constexpr bool kIsNotFocused = false;

  // PERSISITENT* processes are protected from the kernel OOM killer.
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::PERSISTENT,
                          kIsNotFocused, 0)
                   .IsKernelKillable());
  EXPECT_FALSE(ArcProcess(0, 0, "process",
                          mojom::ProcessStateDeprecated::PERSISTENT_UI,
                          kIsNotFocused, 0)
                   .IsKernelKillable());

  // Both TOP+focused and TOP apps are still kernel-killable.
  EXPECT_TRUE(
      ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::TOP, true, 0)
          .IsKernelKillable());
  EXPECT_TRUE(
      ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::TOP, false, 0)
          .IsKernelKillable());

  // Others are kernel-killable.
  EXPECT_TRUE(
      ArcProcess(0, 0, "process",
                 mojom::ProcessStateDeprecated::BOUND_FOREGROUND_SERVICE,
                 kIsNotFocused, 0)
          .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::FOREGROUND_SERVICE,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::TOP_SLEEPING,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::IMPORTANT_FOREGROUND,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::IMPORTANT_BACKGROUND,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::BACKUP,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::HEAVY_WEIGHT,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::SERVICE, kIsNotFocused,
                         0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::RECEIVER, kIsNotFocused,
                         0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process", mojom::ProcessStateDeprecated::HOME,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::LAST_ACTIVITY,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::CACHED_ACTIVITY,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::CACHED_ACTIVITY_CLIENT,
                         kIsNotFocused, 0)
                  .IsKernelKillable());
  EXPECT_TRUE(ArcProcess(0, 0, "process",
                         mojom::ProcessStateDeprecated::CACHED_EMPTY,
                         kIsNotFocused, 0)
                  .IsKernelKillable());

  // Set of custom processes that are protected from the kernel OOM killer.
  EXPECT_FALSE(ArcProcess(0, 0, "com.google.android.apps.work.clouddpc.arc",
                          mojom::ProcessStateDeprecated::SERVICE, kIsNotFocused,
                          0)
                   .IsKernelKillable());
}

// Tests operator<<() does not crash and returns non-empty result, at least.
TEST(ArcProcess, TestStringification) {
  std::stringstream s;
  s << ArcProcess(0, 0, "p", mojom::ProcessStateDeprecated::PERSISTENT, false,
                  0);
  EXPECT_FALSE(s.str().empty());
}

}  // namespace

}  // namespace arc
