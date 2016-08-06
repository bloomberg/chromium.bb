// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/sampling/task_manager_impl.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

// A Task for unittests, not backed by a real process, that can report any given
// value.
class FakeTask : public Task {
 public:
  FakeTask(base::ProcessId process_id,
           Type type,
           const std::string& title,
           int tab_id)
      : Task(base::ASCIIToUTF16(title),
             "FakeTask",
             nullptr,
             base::kNullProcessHandle,
             process_id),
        type_(type),
        parent_(nullptr),
        tab_id_(tab_id) {
    TaskManagerImpl::GetInstance()->TaskAdded(this);
  }

  ~FakeTask() override { TaskManagerImpl::GetInstance()->TaskRemoved(this); }

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return parent_; }

  int GetTabId() const override { return tab_id_; }

  void SetParent(Task* parent) { parent_ = parent; }

 private:
  Type type_;
  Task* parent_;
  int tab_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeTask);
};

}  // namespace

class TaskManagerImplTest : public testing::Test, public TaskManagerObserver {
 public:
  TaskManagerImplTest()
      : TaskManagerObserver(base::TimeDelta::FromSeconds(1),
                            REFRESH_TYPE_NONE) {
    TaskManagerImpl::GetInstance()->AddObserver(this);
  }
  ~TaskManagerImplTest() override {
    tasks_.clear();
    observed_task_manager()->RemoveObserver(this);
  }

  FakeTask* AddTask(int pid_offset,
                    Task::Type type,
                    const std::string& title,
                    int tab_id) {
    // Offset based on the current process id, to avoid collisions with the
    // browser process task.
    base::ProcessId process_id = base::GetCurrentProcId() + pid_offset;
    tasks_.emplace_back(new FakeTask(process_id, type, title, tab_id));
    return tasks_.back().get();
  }

  std::string DumpSortedTasks() {
    std::string result;
    for (TaskId task_id : observed_task_manager()->GetTaskIdsList()) {
      result += base::UTF16ToUTF8(observed_task_manager()->GetTitle(task_id));
      result += "\n";
    }
    return result;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::vector<std::unique_ptr<FakeTask>> tasks_;
  DISALLOW_COPY_AND_ASSIGN(TaskManagerImplTest);
};

TEST_F(TaskManagerImplTest, SortingTypes) {
  AddTask(100, Task::GPU, "Gpu Process", -1);

  Task* tab1 = AddTask(200, Task::RENDERER, "Tab One", 10);
  AddTask(400, Task::EXTENSION, "Extension Subframe: Tab One", 10)
      ->SetParent(tab1);
  AddTask(300, Task::RENDERER, "Subframe: Tab One", 10)->SetParent(tab1);

  Task* tab2 =
      AddTask(200, Task::RENDERER, "Tab Two: sharing process with Tab One", 20);

  AddTask(301, Task::RENDERER, "Subframe: Tab Two", 20)->SetParent(tab2);
  AddTask(400, Task::EXTENSION, "Extension Subframe: Tab Two", 20)
      ->SetParent(tab2);

  AddTask(600, Task::ARC, "ARC", -1);
  AddTask(800, Task::UTILITY, "Utility One", -1);
  AddTask(700, Task::UTILITY, "Utility Two", -1);
  AddTask(1000, Task::GUEST, "Guest", 20);
  AddTask(900, Task::WORKER, "Worker", -1);
  AddTask(500, Task::ZYGOTE, "Zygote", -1);

  AddTask(300, Task::RENDERER, "Subframe: Tab One (2)", 10)->SetParent(tab1);
  AddTask(300, Task::RENDERER, "Subframe: Tab One (third)", 10)
      ->SetParent(tab1);
  AddTask(300, Task::RENDERER, "Subframe: Tab One (4)", 10)->SetParent(tab1);

  EXPECT_EQ(
      "Browser\n"
      "Gpu Process\n"
      "ARC\n"
      "Zygote\n"
      "Utility One\n"
      "Utility Two\n"
      "Tab One\n"
      "Tab Two: sharing process with Tab One\n"
      "Subframe: Tab One\n"
      "Subframe: Tab One (2)\n"
      "Subframe: Tab One (third)\n"
      "Subframe: Tab One (4)\n"
      "Extension Subframe: Tab One\n"
      "Extension Subframe: Tab Two\n"
      "Subframe: Tab Two\n"
      "Guest\n"
      "Worker\n",
      DumpSortedTasks());
}

TEST_F(TaskManagerImplTest, SortingCycles) {
  // Two tabs, with subframes in the other's process. This induces a cycle in
  // the TaskGroup dependencies, without being a cycle in the Tasks. This can
  // happen in practice.
  Task* tab1 = AddTask(200, Task::RENDERER, "Tab 1: Process 200", 10);
  AddTask(300, Task::RENDERER, "Subframe in Tab 1: Process 300", 10)
      ->SetParent(tab1);
  Task* tab2 = AddTask(300, Task::RENDERER, "Tab 2: Process 300", 20);
  AddTask(200, Task::RENDERER, "Subframe in Tab 2: Process 200", 20)
      ->SetParent(tab2);

  // Simulated GPU process.
  AddTask(100, Task::GPU, "Gpu Process", -1);

  // Two subframes that list each other as a parent (a true cycle). This
  // shouldn't happen in practice, but we want the sorting code to handle it
  // gracefully.
  FakeTask* cycle1 = AddTask(501, Task::SANDBOX_HELPER, "Cycle 1", -1);
  FakeTask* cycle2 = AddTask(500, Task::ARC, "Cycle 2", -1);
  cycle1->SetParent(cycle2);
  cycle2->SetParent(cycle1);

  // A cycle where both elements are in the same group.
  FakeTask* cycle3 = AddTask(600, Task::SANDBOX_HELPER, "Cycle 3", -1);
  FakeTask* cycle4 = AddTask(600, Task::ARC, "Cycle 4", -1);
  cycle3->SetParent(cycle4);
  cycle4->SetParent(cycle3);

  // Tasks listing a cycle as their parent.
  FakeTask* lollipop5 = AddTask(701, Task::EXTENSION, "Child of Cycle 3", -1);
  lollipop5->SetParent(cycle3);
  FakeTask* lollipop6 = AddTask(700, Task::PLUGIN, "Child of Cycle 4", -1);
  lollipop6->SetParent(cycle4);

  // A task listing itself as parent.
  FakeTask* self_cycle = AddTask(800, Task::RENDERER, "Self Cycle", 5);
  self_cycle->SetParent(self_cycle);

  // Add a plugin child to tab1 and tab2.
  AddTask(900, Task::PLUGIN, "Plugin: Tab 2", 20)->SetParent(tab1);
  AddTask(901, Task::PLUGIN, "Plugin: Tab 1", 10)->SetParent(tab1);

  // Finish with a normal renderer task.
  AddTask(903, Task::RENDERER, "Tab: Normal Renderer", 30);

  // Cycles should wind up on the bottom of the list.
  EXPECT_EQ(
      "Browser\n"
      "Gpu Process\n"
      "Tab 1: Process 200\n"
      "Subframe in Tab 2: Process 200\n"
      "Tab 2: Process 300\n"
      "Subframe in Tab 1: Process 300\n"
      "Plugin: Tab 1\n"
      "Plugin: Tab 2\n"
      "Tab: Normal Renderer\n"
      "Cycle 2\n"           // ARC
      "Cycle 1\n"           // Child of 2
      "Cycle 4\n"           // ARC; task_id > Cycle 2's
      "Cycle 3\n"           // Same-process child of 4 (SANDBOX_HELPER > ARC)
      "Child of Cycle 4\n"  // Child of 4
      "Child of Cycle 3\n"  // Child of 3
      "Self Cycle\n",       // RENDERER (> ARC)
      DumpSortedTasks());
}

}  // namespace task_manager
