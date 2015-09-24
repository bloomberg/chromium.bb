// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_browsertest_util.h"

#include "base/command_line.h"
#include "base/location.h"
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
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {
namespace browsertest_util {

namespace {

class ResourceChangeObserver : public TaskManagerModelObserver {
 public:
  ResourceChangeObserver(const TaskManagerModel* model,
                         int required_count,
                         const base::string16& title_pattern,
                         ColumnSpecifier column_specifier,
                         size_t min_column_value)
      : model_(model),
        required_count_(required_count),
        title_pattern_(title_pattern),
        column_specifier_(column_specifier),
        min_column_value_(min_column_value) {}

  void OnModelChanged() override { OnResourceChange(); }

  void OnItemsChanged(int start, int length) override { OnResourceChange(); }

  void OnItemsAdded(int start, int length) override { OnResourceChange(); }

  void OnItemsRemoved(int start, int length) override { OnResourceChange(); }

  void RunUntilSatisfied() {
    // See if the condition is satisfied without having to run the loop. This
    // check has to be placed after the installation of the
    // TaskManagerModelObserver, because resources may change before that.
    if (IsSatisfied())
      return;

    timer_.Start(FROM_HERE,
                 TestTimeouts::action_timeout(),
                 this,
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
    for (int i = 0; i < model_->ResourceCount(); i++) {
      if (!base::MatchPattern(model_->GetResourceTitle(i), title_pattern_))
        continue;

      if (GetColumnValue(i) < min_column_value_)
        continue;

      match_count++;
    }
    return match_count;
  }

  size_t GetColumnValue(int index) {
    size_t value = 0;
    bool success = false;
    switch (column_specifier_) {
      case COLUMN_NONE:
        break;
      case V8_MEMORY:
        success = model_->GetV8Memory(index, &value);
        break;
      case V8_MEMORY_USED:
        success = model_->GetV8MemoryUsed(index, &value);
        break;
      case SQLITE_MEMORY_USED:
        success = model_->GetSqliteMemoryUsedBytes(index, &value);
        break;
    }
    if (!success)
      return 0;
    return value;
  }

  const char* GetColumnName() {
    switch (column_specifier_) {
      case COLUMN_NONE:
        return "N/A";
      case V8_MEMORY:
        return "V8 Memory";
      case V8_MEMORY_USED:
        return "V8 Memory Used";
      case SQLITE_MEMORY_USED:
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
    for (int i = 0; i < model_->ResourceCount(); i++) {
      task_manager_state_dump
          << "\n  > " << std::setw(40) << std::left
          << base::UTF16ToASCII(model_->GetResourceTitle(i));
      if (min_column_value_ > 0) {
        task_manager_state_dump << " [" << GetColumnName()
                                << " == " << GetColumnValue(i) << "]";
      }
    }
    return task_manager_state_dump;
  }

  const TaskManagerModel* model_;
  const int required_count_;
  const base::string16 title_pattern_;
  const ColumnSpecifier column_specifier_;
  const size_t min_column_value_;
  base::RunLoop run_loop_;
  base::OneShotTimer timer_;
};

}  // namespace

void EnableOldTaskManager() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableNewTaskManager);
}

void WaitForTaskManagerRows(int required_count,
                            const base::string16& title_pattern) {
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  const int column_value_dont_care = 0;
  ResourceChangeObserver observer(model, required_count, title_pattern,
                                  COLUMN_NONE, column_value_dont_care);
  model->AddObserver(&observer);
  observer.RunUntilSatisfied();
  model->RemoveObserver(&observer);
}

void WaitForTaskManagerStatToExceed(const base::string16& title_pattern,
                                    ColumnSpecifier column_getter,
                                    size_t min_column_value) {
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  const int wait_for_one_match = 1;
  ResourceChangeObserver observer(model, wait_for_one_match, title_pattern,
                                  column_getter, min_column_value);
  model->AddObserver(&observer);
  observer.RunUntilSatisfied();
  model->RemoveObserver(&observer);
}

base::string16 MatchTab(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyTab() { return MatchTab("*"); }

base::string16 MatchAboutBlankTab() { return MatchTab("about:blank"); }

base::string16 MatchExtension(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_EXTENSION_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyExtension() { return MatchExtension("*"); }

base::string16 MatchApp(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_APP_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyApp() { return MatchApp("*"); }

base::string16 MatchWebView(const char* title) {
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_TASK_MANAGER_WEBVIEW_TAG_PREFIX,
      base::ASCIIToUTF16(title));
}

base::string16 MatchAnyWebView() { return MatchWebView("*"); }

base::string16 MatchBackground(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyBackground() { return MatchBackground("*"); }

base::string16 MatchPrint(const char* title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    base::ASCIIToUTF16(title));
}

base::string16 MatchAnyPrint() { return MatchPrint("*"); }

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
