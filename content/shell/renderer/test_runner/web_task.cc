// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/web_task.h"

#include <algorithm>

#include "third_party/WebKit/public/web/WebKit.h"

namespace content {

WebTask::WebTask(WebTaskList* list) : task_list_(list) {
  task_list_->RegisterTask(this);
}

WebTask::~WebTask() {
  if (task_list_)
    task_list_->UnregisterTask(this);
}

WebTaskList::WebTaskList() {
}

WebTaskList::~WebTaskList() {
  RevokeAll();
}

void WebTaskList::RegisterTask(WebTask* task) {
  tasks_.push_back(task);
}

void WebTaskList::UnregisterTask(WebTask* task) {
  std::vector<WebTask*>::iterator iter =
      std::find(tasks_.begin(), tasks_.end(), task);
  if (iter != tasks_.end())
    tasks_.erase(iter);
}

void WebTaskList::RevokeAll() {
  while (!tasks_.empty())
    tasks_[0]->cancel();
}

}  // namespace content
