// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_TASK_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_TASK_MANAGER_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "content/public/browser/web_ui_message_handler.h"
#include "chrome/browser/task_manager/task_manager.h"

namespace base {
class ListValue;
class Value;
}

class TaskManagerHandler : public content::WebUIMessageHandler,
                           public TaskManagerModelObserver {
 public:
  explicit TaskManagerHandler(TaskManager* tm);
  virtual ~TaskManagerHandler();

  // TaskManagerModelObserver implementation.
  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() OVERRIDE;
  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  // Invoked when new items are added.
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(int start, int length) OVERRIDE;

  // Invoked when the initialization of the model has been finished and
  // periodic updates is started.
  virtual void OnReadyPeriodicalUpdate() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "killProcesses" message.
  void HandleKillProcesses(const base::ListValue* indexes);

  // Callback for the "activatePage" message.
  void HandleActivatePage(const base::ListValue* resource_index);

  // Callback for the "inspect" message.
  void HandleInspect(const base::ListValue* resource_index);

  void EnableTaskManager(const base::ListValue* indexes);
  void DisableTaskManager(const base::ListValue* indexes);
  void OpenAboutMemory(const base::ListValue* indexes);

  // Callback for the "setUpdateColumn" message.
  void HandleSetUpdateColumn(const base::ListValue* args);

 private:
  bool is_alive();

  // Models
  TaskManager* task_manager_;
  TaskManagerModel* model_;

  bool is_enabled_;

  // Set to store the enabled columns.
  std::set<std::string> enabled_columns_;

  // Invoked when group(s) are added/changed/removed.
  // These method are called from OnItemAdded/-Changed/-Removed internally.
  void OnGroupAdded(int start, int length);
  void OnGroupChanged(int start, int length);
  void OnGroupRemoved(int start, int length);

  // Creates or updates information for a single task group. A task group is a
  // group of tasks associated with a single process ID. |group_index| is the
  // integer index of the target process within the data model.
  base::DictionaryValue* CreateTaskGroupValue(int group_index);

  // Creates a list of values to display within a single column of the task
  // manager for a single task group. |columnn_name| is the name of the column.
  // |index| is the index of the resources associated with a process group.
  // |length| is the number of values associated with the group, and is either
  // 1 if all tasks within the group share a common value or equal to the
  // number of tasks within the group.
  void CreateGroupColumnList(const std::string& column_name,
                             const int index,
                             const int length,
                             base::DictionaryValue* val);

  // Retrieves the value of a property for a single task. |column_name| is the
  // name of the property, which is associated with a column within the task
  // manager or "about memory" page. |i| is the index of the task. Tasks are
  // grouped by process ID, and the index of the task is the sum of the group
  // offset and the index of the task within the group.
  base::Value* CreateColumnValue(const std::string& column_name,
                                 const int i);

  DISALLOW_COPY_AND_ASSIGN(TaskManagerHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_TASK_MANAGER_HANDLER_H_
