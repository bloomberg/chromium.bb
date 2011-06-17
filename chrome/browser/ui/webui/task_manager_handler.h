// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_HANDLER_H_
#pragma once

#include "content/browser/webui/web_ui.h"
#include "chrome/browser/task_manager/task_manager.h"

class ListValue;

class TaskManagerHandler : public WebUIMessageHandler,
                           public TaskManagerModelObserver {
 public:
  explicit TaskManagerHandler(TaskManager* tm);
  virtual ~TaskManagerHandler();

  void Init();

  // TaskManagerModelObserver implementation.
  // Invoked when the model has been completely changed.
  virtual void OnModelChanged() OVERRIDE;
  // Invoked when a range of items has changed.
  virtual void OnItemsChanged(int start, int length) OVERRIDE;
  // Invoked when new items are added.
  virtual void OnItemsAdded(int start, int length) OVERRIDE;
  // Invoked when a range of items has been removed.
  virtual void OnItemsRemoved(int start, int length) OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "killProcess" message.
  void HandleKillProcess(const ListValue* args);

  void EnableTaskManager(const ListValue* indexes);
  void DisableTaskManager(const ListValue* indexes);
  void OpenAboutMemory(const ListValue* indexes);

 private:
  // Models
  TaskManager* task_manager_;
  TaskManagerModel* model_;

  bool is_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_HANDLER_H_
