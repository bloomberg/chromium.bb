// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_browsertest_util.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {
namespace browsertest_util {

namespace {

class ResourceChangeObserver : public TaskManagerModelObserver {
 public:
  ResourceChangeObserver(const TaskManagerModel* model,
                         int required_count,
                         const base::string16& title_pattern)
      : model_(model),
        required_count_(required_count),
        title_pattern_(title_pattern) {}

  virtual void OnModelChanged() OVERRIDE {
    OnResourceChange();
  }

  virtual void OnItemsChanged(int start, int length) OVERRIDE {
    OnResourceChange();
  }

  virtual void OnItemsAdded(int start, int length) OVERRIDE {
    OnResourceChange();
  }

  virtual void OnItemsRemoved(int start, int length) OVERRIDE {
    OnResourceChange();
  }

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

    base::MessageLoop::current()->PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

  bool IsSatisfied() { return CountMatches() == required_count_; }

  int CountMatches() {
    int match_count = 0;
    for (int i = 0; i < model_->ResourceCount(); i++) {
      task_manager::Resource::Type type = model_->GetResourceType(i);
      // Skip system infrastructure resources.
      if (type == task_manager::Resource::BROWSER ||
          type == task_manager::Resource::NACL ||
          type == task_manager::Resource::GPU ||
          type == task_manager::Resource::UTILITY ||
          type == task_manager::Resource::ZYGOTE ||
          type == task_manager::Resource::SANDBOX_HELPER) {
        continue;
      }

      if (MatchPattern(model_->GetResourceTitle(i), title_pattern_)) {
        match_count++;
      }
    }
    return match_count;
  }

  void OnTimeout() {
    base::MessageLoop::current()->PostTask(FROM_HERE, run_loop_.QuitClosure());
    FAIL() << "Timed out.\n" << DumpTaskManagerModel();
  }

  testing::Message DumpTaskManagerModel() {
    testing::Message task_manager_state_dump;
    task_manager_state_dump << "Waiting for exactly " << required_count_
                            << " matches of wildcard pattern \""
                            << base::UTF16ToASCII(title_pattern_) << "\"\n";
    task_manager_state_dump << "Currently there are " << CountMatches()
                            << " matches.\n";
    task_manager_state_dump << "Current Task Manager Model is:\n";
    for (int i = 0; i < model_->ResourceCount(); i++) {
      task_manager_state_dump
          << "  > " << base::UTF16ToASCII(model_->GetResourceTitle(i)) << "\n";
    }
    return task_manager_state_dump;
  }

  const TaskManagerModel* model_;
  const int required_count_;
  const base::string16 title_pattern_;
  base::RunLoop run_loop_;
  base::OneShotTimer<ResourceChangeObserver> timer_;
};

}  // namespace

void WaitForTaskManagerRows(int required_count,
                            const base::string16& title_pattern) {
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  ResourceChangeObserver observer(model, required_count, title_pattern);
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
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_WEBVIEW_TAG_PREFIX,
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

}  // namespace browsertest_util
}  // namespace task_manager
