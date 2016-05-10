// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_browsertest_util.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/task_manager/task_manager_table_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

namespace task_manager {
namespace browsertest_util {

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

namespace {

class LegacyTaskManagerTesterImpl : public TaskManagerTester,
                                    public TaskManagerModelObserver {
 public:
  explicit LegacyTaskManagerTesterImpl(const base::Closure& on_resource_change)
      : on_resource_change_(on_resource_change),
        model_(TaskManager::GetInstance()->model()) {
    if (!on_resource_change_.is_null())
      model_->AddObserver(this);
  }
  ~LegacyTaskManagerTesterImpl() override {
    if (!on_resource_change_.is_null())
      model_->RemoveObserver(this);
  }

  // TaskManagerTester:
  int GetRowCount() override { return model_->ResourceCount(); }

  base::string16 GetRowTitle(int row) override {
    return model_->GetResourceTitle(row);
  }

  int64_t GetColumnValue(ColumnSpecifier column, int row) override {
    size_t value = 0;
    bool success = false;
    switch (column) {
      case ColumnSpecifier::COLUMN_NONE:
        break;
      case ColumnSpecifier::V8_MEMORY:
        success = model_->GetV8Memory(row, &value);
        break;
      case ColumnSpecifier::V8_MEMORY_USED:
        success = model_->GetV8MemoryUsed(row, &value);
        break;
      case ColumnSpecifier::SQLITE_MEMORY_USED:
        success = model_->GetSqliteMemoryUsedBytes(row, &value);
        break;
    }
    if (!success)
      return 0;
    return static_cast<int64_t>(value);
  }

  void ToggleColumnVisibility(ColumnSpecifier column) override {
    // Doing nothing is okay here; the legacy TaskManager always collects all
    // stats.
  }

  int32_t GetTabId(int row) override {
    if (model_->GetResourceWebContents(row)) {
      return SessionTabHelper::IdForTab(model_->GetResourceWebContents(row));
    }
    return -1;
  }

  void Kill(int row) override { TaskManager::GetInstance()->KillProcess(row); }

  // TaskManagerModelObserver:
  void OnModelChanged() override { OnResourceChange(); }
  void OnItemsChanged(int start, int length) override { OnResourceChange(); }
  void OnItemsAdded(int start, int length) override { OnResourceChange(); }
  void OnItemsRemoved(int start, int length) override { OnResourceChange(); }

 private:
  void OnResourceChange() {
    if (!on_resource_change_.is_null())
      on_resource_change_.Run();
  }
  base::Closure on_resource_change_;
  TaskManagerModel* model_;
};

// Helper class to run a message loop until a TaskManagerTester is in an
// expected state. If timeout occurs, an ASCII version of the task manager's
// contents, along with a summary of the expected state, are dumped to test
// output, to assist debugging.
class ResourceChangeObserver {
 public:
  ResourceChangeObserver(int required_count,
                         const base::string16& title_pattern,
                         ColumnSpecifier column_specifier,
                         size_t min_column_value)
      : required_count_(required_count),
        title_pattern_(title_pattern),
        column_specifier_(column_specifier),
        min_column_value_(min_column_value) {
    base::Closure callback = base::Bind(
        &ResourceChangeObserver::OnResourceChange, base::Unretained(this));

    if (IsNewTaskManagerViewEnabled())
      task_manager_tester_.reset(new TaskManagerTesterImpl(callback));
    else
      task_manager_tester_.reset(new LegacyTaskManagerTesterImpl(callback));
  }

  void RunUntilSatisfied() {
    // See if the condition is satisfied without having to run the loop. This
    // check has to be placed after the installation of the
    // TaskManagerModelObserver, because resources may change before that.
    if (IsSatisfied())
      return;

    timer_.Start(FROM_HERE, TestTimeouts::action_timeout(), this,
                 &ResourceChangeObserver::OnTimeout);

    run_loop_.Run();

    // If we succeeded normally (no timeout), check our post condition again
    // before returning control to the test. If it is no longer satisfied, the
    // test is likely flaky: we were waiting for a state that was only achieved
    // emphemerally), so treat this as a failure.
    if (!IsSatisfied() && timer_.IsRunning()) {
      FAIL() << "Wait condition satisfied only emphemerally. Likely test "
             << "problem. Maybe wait instead for the state below?\n"
             << DumpTaskManagerModel();
    }

    timer_.Stop();
  }

 private:
  void OnResourceChange() {
    if (!IsSatisfied())
      return;

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

  bool IsSatisfied() { return CountMatches() == required_count_; }

  int CountMatches() {
    int match_count = 0;
    for (int i = 0; i < task_manager_tester_->GetRowCount(); i++) {
      if (!base::MatchPattern(task_manager_tester_->GetRowTitle(i),
                              title_pattern_))
        continue;

      if (GetColumnValue(i) < min_column_value_)
        continue;

      match_count++;
    }
    return match_count;
  }

  int64_t GetColumnValue(int index) {
    return task_manager_tester_->GetColumnValue(column_specifier_, index);
  }

  const char* GetColumnName() {
    switch (column_specifier_) {
      case ColumnSpecifier::COLUMN_NONE:
        return "N/A";
      case ColumnSpecifier::V8_MEMORY:
        return "V8 Memory";
      case ColumnSpecifier::V8_MEMORY_USED:
        return "V8 Memory Used";
      case ColumnSpecifier::SQLITE_MEMORY_USED:
        return "SQLite Memory Used";
    }
    return "N/A";
  }

  void OnTimeout() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
    FAIL() << "Timed out.\n" << DumpTaskManagerModel();
  }

  testing::Message DumpTaskManagerModel() {
    testing::Message task_manager_state_dump;
    task_manager_state_dump << "Waiting for exactly " << required_count_
                            << " matches of wildcard pattern \""
                            << base::UTF16ToASCII(title_pattern_) << "\"";
    if (min_column_value_ > 0) {
      task_manager_state_dump << " && [" << GetColumnName()
                              << " >= " << min_column_value_ << "]";
    }
    task_manager_state_dump << "\nCurrently there are " << CountMatches()
                            << " matches.";
    task_manager_state_dump << "\nCurrent Task Manager Model is:";
    for (int i = 0; i < task_manager_tester_->GetRowCount(); i++) {
      task_manager_state_dump
          << "\n  > " << std::setw(40) << std::left
          << base::UTF16ToASCII(task_manager_tester_->GetRowTitle(i));
      if (min_column_value_ > 0) {
        task_manager_state_dump << " [" << GetColumnName()
                                << " == " << GetColumnValue(i) << "]";
      }
    }
    return task_manager_state_dump;
  }

  std::unique_ptr<TaskManagerTester> task_manager_tester_;
  const int required_count_;
  const base::string16 title_pattern_;
  const ColumnSpecifier column_specifier_;
  const int64_t min_column_value_;
  base::RunLoop run_loop_;
  base::OneShotTimer timer_;
};

}  // namespace

std::unique_ptr<TaskManagerTester> GetTaskManagerTester() {
  if (IsNewTaskManagerViewEnabled())
    return base::WrapUnique(new TaskManagerTesterImpl(base::Closure()));
  else
    return base::WrapUnique(new LegacyTaskManagerTesterImpl(base::Closure()));
}

void WaitForTaskManagerRows(int required_count,
                            const base::string16& title_pattern) {
  const int column_value_dont_care = 0;
  ResourceChangeObserver observer(required_count, title_pattern,
                                  ColumnSpecifier::COLUMN_NONE,
                                  column_value_dont_care);
  observer.RunUntilSatisfied();
}

void WaitForTaskManagerStatToExceed(const base::string16& title_pattern,
                                    ColumnSpecifier column_getter,
                                    size_t min_column_value) {
  const int wait_for_one_match = 1;
  ResourceChangeObserver observer(wait_for_one_match, title_pattern,
                                  column_getter, min_column_value);
  observer.RunUntilSatisfied();
}

base::string16 MatchTab(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyTab() {
  return MatchTab("*");
}

base::string16 MatchAboutBlankTab() {
  return MatchTab("about:blank");
}

base::string16 MatchExtension(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_EXTENSION_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyExtension() {
  return MatchExtension("*");
}

base::string16 MatchApp(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_APP_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyApp() {
  return MatchApp("*");
}

base::string16 MatchWebView(const char* title) {
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_TASK_MANAGER_WEBVIEW_TAG_PREFIX, base::ASCIIToUTF16(title));
}

base::string16 MatchAnyWebView() {
  return MatchWebView("*");
}

base::string16 MatchBackground(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyBackground() {
  return MatchBackground("*");
}

base::string16 MatchPrint(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyPrint() {
  return MatchPrint("*");
}

base::string16 MatchSubframe(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_SUBFRAME_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnySubframe() {
  return MatchSubframe("*");
}

base::string16 MatchUtility(const base::string16& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX, title);
}

base::string16 MatchAnyUtility() {
  return MatchUtility(base::ASCIIToUTF16("*"));
}

}  // namespace browsertest_util
}  // namespace task_manager
