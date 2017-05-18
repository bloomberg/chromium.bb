// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/task_tracker.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::SizeIs;

namespace sync_sessions {

namespace {
const int kTab1 = 15;
const int kTab2 = 25;
}  // namespace

TEST(TaskTrackerTest, GetTabTasks) {
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);
  ASSERT_NE(tab_tasks, nullptr);
  EXPECT_EQ(task_tracker.GetTabTasks(kTab1), tab_tasks);
  EXPECT_NE(task_tracker.GetTabTasks(kTab2), tab_tasks);
}

TEST(TaskTrackerTest, CleanTabTasks) {
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);
  ASSERT_NE(tab_tasks, nullptr);
  ASSERT_FALSE(task_tracker.local_tab_tasks_map_.empty());

  task_tracker.CleanTabTasks(kTab1);
  EXPECT_TRUE(task_tracker.local_tab_tasks_map_.empty());
}

TEST(TaskTrackerTest, UpdateTasksWithMultipleClicks) {
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);

  tab_tasks->UpdateWithNavigation(1, ui::PageTransition::PAGE_TRANSITION_TYPED,
                                  100);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));

  tab_tasks->UpdateWithNavigation(2, ui::PageTransition::PAGE_TRANSITION_LINK,
                                  200);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(100, 200));

  tab_tasks->UpdateWithNavigation(3, ui::PageTransition::PAGE_TRANSITION_LINK,
                                  300);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(100, 200));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(3),
              ElementsAre(100, 200, 300));
}

TEST(TaskTrackerTest, UpdateTasksWithMultipleClicksAndTypes) {
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);

  tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK, 100);
  tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 200);
  tab_tasks->UpdateWithNavigation(3, ui::PAGE_TRANSITION_LINK, 300);
  tab_tasks->UpdateWithNavigation(4, ui::PAGE_TRANSITION_TYPED, 400);
  tab_tasks->UpdateWithNavigation(5, ui::PAGE_TRANSITION_LINK, 500);
  tab_tasks->UpdateWithNavigation(6, ui::PAGE_TRANSITION_TYPED, 600);
  tab_tasks->UpdateWithNavigation(7, ui::PAGE_TRANSITION_LINK, 700);

  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(100, 200));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(3),
              ElementsAre(100, 200, 300));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(4), ElementsAre(400));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(5), ElementsAre(400, 500));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(6), ElementsAre(600));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(7), ElementsAre(600, 700));
}

TEST(TaskTrackerTest, UpdateTasksWithBackforwards) {
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);

  tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_TYPED, 100);
  tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 200);
  tab_tasks->UpdateWithNavigation(3, ui::PAGE_TRANSITION_LINK, 300);

  tab_tasks->UpdateWithNavigation(
      1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      400);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(100, 200));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(3),
              ElementsAre(100, 200, 300));

  tab_tasks->UpdateWithNavigation(
      3,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      500);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(100, 200));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(3),
              ElementsAre(100, 200, 300));
}

TEST(TaskTrackerTest, UpdateWithNavigationsWithBackAndForkedNavigation) {
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);
  tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK, 100);
  tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 200);
  tab_tasks->UpdateWithNavigation(3, ui::PAGE_TRANSITION_LINK, 300);
  tab_tasks->UpdateWithNavigation(
      1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      400);
  tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 500);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(100, 500));
  // We don't track navigation at index 3 any more, and it's out of scope now.
  EXPECT_THAT(tab_tasks->GetNavigationsCount(), 3);
}

TEST(TaskTrackerTest, LimitMaxNumberOfTasksPerTab) {
  int kMaxNumTasksPerTab = 100;
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);

  // Reaching max number of tasks for a tab.
  for (int i = 0; i < kMaxNumTasksPerTab; i++) {
    tab_tasks->UpdateWithNavigation(i, ui::PAGE_TRANSITION_LINK, i * 100);
  }

  tab_tasks->UpdateWithNavigation(kMaxNumTasksPerTab, ui::PAGE_TRANSITION_LINK,
                                  kMaxNumTasksPerTab * 100);

  ASSERT_THAT(tab_tasks->task_ids_, SizeIs(kMaxNumTasksPerTab));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(0), ElementsAre());
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre(100));
  std::vector<int64_t> task_ids =
      tab_tasks->GetTaskIdsForNavigation(kMaxNumTasksPerTab);
  EXPECT_THAT(task_ids, SizeIs(kMaxNumTasksPerTab));
  EXPECT_EQ(task_ids[0], 100);
  EXPECT_EQ(task_ids[kMaxNumTasksPerTab - 1], kMaxNumTasksPerTab * 100);

  tab_tasks->UpdateWithNavigation(kMaxNumTasksPerTab + 1,
                                  ui::PAGE_TRANSITION_LINK,
                                  (kMaxNumTasksPerTab + 1) * 100);

  ASSERT_THAT(tab_tasks->task_ids_, SizeIs(kMaxNumTasksPerTab));
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(0), ElementsAre());
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1), ElementsAre());
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(2), ElementsAre(200));
  task_ids = tab_tasks->GetTaskIdsForNavigation(kMaxNumTasksPerTab + 1);
  EXPECT_THAT(task_ids, SizeIs(kMaxNumTasksPerTab));
  EXPECT_EQ(task_ids[0], 200);
  EXPECT_EQ(task_ids[kMaxNumTasksPerTab - 1], (kMaxNumTasksPerTab + 1) * 100);
}

TEST(TaskTrackerTest, CreateSubTaskFromExcludedAncestorTask) {
  int kMaxNumTasksPerTab = 100;
  TaskTracker task_tracker;
  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab1);

  // Reaching max number of tasks for a tab.
  for (int i = 0; i < kMaxNumTasksPerTab; i++) {
    tab_tasks->UpdateWithNavigation(i, ui::PAGE_TRANSITION_LINK, i * 100);
  }

  tab_tasks->UpdateWithNavigation(kMaxNumTasksPerTab, ui::PAGE_TRANSITION_LINK,
                                  kMaxNumTasksPerTab * 100);
  ASSERT_EQ(tab_tasks->excluded_navigation_num_, 1);
  ASSERT_EQ(tab_tasks->current_navigation_index_, kMaxNumTasksPerTab);

  tab_tasks->UpdateWithNavigation(
      0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      (kMaxNumTasksPerTab + 1) * 100);
  ASSERT_EQ(tab_tasks->excluded_navigation_num_, 1);
  ASSERT_EQ(tab_tasks->current_navigation_index_, 0);

  tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK,
                                  (kMaxNumTasksPerTab + 2) * 100);
  ASSERT_THAT(tab_tasks->task_ids_, SizeIs(1));
  ASSERT_EQ(tab_tasks->GetNavigationsCount(), 2);
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(0), ElementsAre());
  EXPECT_THAT(tab_tasks->GetTaskIdsForNavigation(1),
              ElementsAre((kMaxNumTasksPerTab + 2) * 100));
}

TEST(TaskTrackerTest, CreateTabTasksFromSourceTab) {
  TaskTracker task_tracker;
  TabTasks* source_tab_tasks = task_tracker.GetTabTasks(kTab1);
  source_tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK, 100);
  source_tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 200);
  source_tab_tasks->UpdateWithNavigation(3, ui::PAGE_TRANSITION_TYPED, 300);
  source_tab_tasks->UpdateWithNavigation(4, ui::PAGE_TRANSITION_LINK, 400);
  source_tab_tasks->UpdateWithNavigation(5, ui::PAGE_TRANSITION_LINK, 500);

  TabTasks* target_tab_tasks = task_tracker.GetTabTasks(kTab2, kTab1);
  target_tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK, 600);
  target_tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 700);
  target_tab_tasks->UpdateWithNavigation(3, ui::PAGE_TRANSITION_TYPED, 800);

  EXPECT_THAT(target_tab_tasks->GetTaskIdsForNavigation(1),
              ElementsAre(300, 400, 500, 600));
  EXPECT_THAT(target_tab_tasks->GetTaskIdsForNavigation(2),
              ElementsAre(300, 400, 500, 600, 700));
  EXPECT_THAT(target_tab_tasks->GetTaskIdsForNavigation(3), ElementsAre(800));
}

TEST(TaskTrackerTest, CreateTabTasksFromSourceTabAfterGoingBack) {
  TaskTracker task_tracker;
  TabTasks* source_tab_tasks = task_tracker.GetTabTasks(kTab1);
  source_tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK, 100);
  source_tab_tasks->UpdateWithNavigation(2, ui::PAGE_TRANSITION_LINK, 200);
  source_tab_tasks->UpdateWithNavigation(3, ui::PAGE_TRANSITION_TYPED, 300);
  source_tab_tasks->UpdateWithNavigation(4, ui::PAGE_TRANSITION_LINK, 400);
  source_tab_tasks->UpdateWithNavigation(5, ui::PAGE_TRANSITION_LINK, 500);
  source_tab_tasks->UpdateWithNavigation(
      2,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      600);
  ASSERT_THAT(source_tab_tasks->GetTaskIdsForNavigation(2),
              ElementsAre(100, 200));

  TabTasks* target_tab_tasks = task_tracker.GetTabTasks(kTab2, kTab1);
  target_tab_tasks->UpdateWithNavigation(1, ui::PAGE_TRANSITION_LINK, 700);

  EXPECT_THAT(target_tab_tasks->GetTaskIdsForNavigation(1),
              ElementsAre(100, 200, 700));
}

TEST(TaskTrackerTest, CreateTabTasksFromSourceTabWithLimitedTaskNum) {
  const int kMaxNumTasksPerTab = 100;
  TaskTracker task_tracker;

  TabTasks* source_tab_tasks = task_tracker.GetTabTasks(kTab1);
  // Adding max number of tasks to tab1.
  for (int i = 0; i < kMaxNumTasksPerTab; i++) {
    source_tab_tasks->UpdateWithNavigation(i, ui::PAGE_TRANSITION_LINK,
                                           i * 100);
  }

  TabTasks* tab_tasks = task_tracker.GetTabTasks(kTab2, kTab1);
  tab_tasks->UpdateWithNavigation(0, ui::PAGE_TRANSITION_LINK,
                                  kMaxNumTasksPerTab * 100);

  std::vector<int64_t> task_ids = tab_tasks->GetTaskIdsForNavigation(0);
  // The first task from source tab is excluded because of max task number
  // limit.
  int expected_task_ids[kMaxNumTasksPerTab];
  for (int i = 0; i < kMaxNumTasksPerTab; i++)
    expected_task_ids[i] = (i + 1) * 100;
  EXPECT_THAT(task_ids,
              ElementsAreArray(expected_task_ids, kMaxNumTasksPerTab));
}

}  // namespace sync_sessions
