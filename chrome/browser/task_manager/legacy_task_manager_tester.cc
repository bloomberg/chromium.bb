// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/legacy_task_manager_tester.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/strings/grit/extensions_strings.h"

namespace task_manager {

class LegacyTaskManagerTester : public task_management::TaskManagerTester,
                                public TaskManagerModelObserver {
 public:
  explicit LegacyTaskManagerTester(const base::Closure& on_resource_change)
      : on_resource_change_(on_resource_change),
        model_(TaskManager::GetInstance()->model()) {
    if (!on_resource_change_.is_null())
      model_->AddObserver(this);
  }
  ~LegacyTaskManagerTester() override {
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

std::unique_ptr<task_management::TaskManagerTester>
CreateLegacyTaskManagerTester(const base::Closure& callback) {
  return base::WrapUnique(new LegacyTaskManagerTester(callback));
}

}  // namespace task_manager
