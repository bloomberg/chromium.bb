// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/task_manager_tester.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "chrome/browser/task_manager/legacy_task_manager_tester.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model_observer.h"

namespace task_management {

namespace {

// Returns whether chrome::ShowTaskManager() will, for the current platform and
// command line, show a view backed by a task_management::TaskManagerTableModel.
bool IsNewTaskManagerViewEnabled() {
#if defined(OS_MACOSX)
  if (!chrome::ToolkitViewsDialogsEnabled())
    return false;
#endif
  return switches::NewTaskManagerEnabled();
}

// Temporarily intercepts the calls between a TableModel and its Observer,
// running |callback| whenever anything happens.
class ScopedInterceptTableModelObserver : public ui::TableModelObserver {
 public:
  ScopedInterceptTableModelObserver(
      ui::TableModel* model_to_intercept,
      ui::TableModelObserver* real_table_model_observer,
      const base::Closure& callback)
      : model_to_intercept_(model_to_intercept),
        real_table_model_observer_(real_table_model_observer),
        callback_(callback) {
    model_to_intercept_->SetObserver(this);
  }

  ~ScopedInterceptTableModelObserver() override {
    model_to_intercept_->SetObserver(real_table_model_observer_);
  }

  // ui::TableModelObserver:
  void OnModelChanged() override {
    real_table_model_observer_->OnModelChanged();
    callback_.Run();
  }
  void OnItemsChanged(int start, int length) override {
    real_table_model_observer_->OnItemsChanged(start, length);
    callback_.Run();
  }
  void OnItemsAdded(int start, int length) override {
    real_table_model_observer_->OnItemsAdded(start, length);
    callback_.Run();
  }
  void OnItemsRemoved(int start, int length) override {
    real_table_model_observer_->OnItemsRemoved(start, length);
    callback_.Run();
  }

 private:
  ui::TableModel* model_to_intercept_;
  ui::TableModelObserver* real_table_model_observer_;
  base::Closure callback_;
};

}  // namespace

// Implementation of TaskManagerTester for the 'new' TaskManager.
class TaskManagerTesterImpl : public TaskManagerTester {
 public:
  explicit TaskManagerTesterImpl(const base::Closure& on_resource_change)
      : model_(GetRealModel()) {
    // Eavesdrop the model->view conversation, since the model only supports
    // single observation.
    if (!on_resource_change.is_null()) {
      interceptor_.reset(new ScopedInterceptTableModelObserver(
          model_, model_->table_model_observer_, on_resource_change));
    }
  }

  ~TaskManagerTesterImpl() override {
    CHECK_EQ(GetRealModel(), model_) << "Task Manager should not be hidden "
                                        "while TaskManagerTester is alive. "
                                        "This indicates a test bug.";
  }

  // TaskManagerTester:
  int GetRowCount() override { return model_->RowCount(); }

  base::string16 GetRowTitle(int row) override {
    return model_->GetText(row, IDS_TASK_MANAGER_TASK_COLUMN);
  }

  void ToggleColumnVisibility(ColumnSpecifier column) override {
    int column_id = 0;
    switch (column) {
      case ColumnSpecifier::COLUMN_NONE:
        return;
      case ColumnSpecifier::SQLITE_MEMORY_USED:
        column_id = IDS_TASK_MANAGER_SQLITE_MEMORY_USED_COLUMN;
        break;
      case ColumnSpecifier::V8_MEMORY_USED:
      case ColumnSpecifier::V8_MEMORY:
        column_id = IDS_TASK_MANAGER_JAVASCRIPT_MEMORY_ALLOCATED_COLUMN;
        break;
    }
    model_->ToggleColumnVisibility(column_id);
  }

  int64_t GetColumnValue(ColumnSpecifier column, int row) override {
    task_management::TaskId task_id = model_->tasks_[row];
    int64_t value = 0;
    int64_t ignored = 0;
    bool success = false;

    switch (column) {
      case ColumnSpecifier::COLUMN_NONE:
        break;
      case ColumnSpecifier::V8_MEMORY:
        success = task_manager()->GetV8Memory(task_id, &value, &ignored);
        break;
      case ColumnSpecifier::V8_MEMORY_USED:
        success = task_manager()->GetV8Memory(task_id, &ignored, &value);
        break;
      case ColumnSpecifier::SQLITE_MEMORY_USED:
        value = task_manager()->GetSqliteMemoryUsed(task_id);
        success = true;
        break;
    }
    if (!success)
      return 0;
    return value;
  }

  int32_t GetTabId(int row) override {
    task_management::TaskId task_id = model_->tasks_[row];
    return task_manager()->GetTabId(task_id);
  }

  void Kill(int row) override { model_->KillTask(row); }

 private:
  task_management::TaskManagerInterface* task_manager() {
    return model_->observed_task_manager();
  }

  // Returns the TaskManagerTableModel for the the visible NewTaskManagerView.
  static task_management::TaskManagerTableModel* GetRealModel() {
    CHECK(IsNewTaskManagerViewEnabled());
    // This downcast is safe, as long as the new task manager is enabled.
    task_management::TaskManagerTableModel* result =
        static_cast<task_management::TaskManagerTableModel*>(
            chrome::ShowTaskManager(nullptr));
    return result;
  }

  task_management::TaskManagerTableModel* model_;
  std::unique_ptr<ScopedInterceptTableModelObserver> interceptor_;
};

// static
std::unique_ptr<TaskManagerTester> TaskManagerTester::Create(
    const base::Closure& callback) {
  if (IsNewTaskManagerViewEnabled())
    return base::WrapUnique(new TaskManagerTesterImpl(callback));
  else
    return task_manager::CreateLegacyTaskManagerTester(callback);
}

}  // namespace task_management
