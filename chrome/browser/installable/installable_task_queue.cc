// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_task_queue.h"

InstallableTaskQueue::InstallableTaskQueue() {}
InstallableTaskQueue::~InstallableTaskQueue() {}

void InstallableTaskQueue::Add(InstallableTask task) {
  tasks_.push_back(task);
}

void InstallableTaskQueue::Reset() {
  tasks_.clear();
  paused_tasks_.clear();
}

void InstallableTaskQueue::UnpauseAll() {
  for (const auto& task : paused_tasks_)
    Add(task);

  paused_tasks_.clear();
}

InstallableTask& InstallableTaskQueue::Current() {
  DCHECK(!tasks_.empty());
  return tasks_[0];
}

void InstallableTaskQueue::PauseCurrent() {
  paused_tasks_.push_back(Current());
  Next();
}

void InstallableTaskQueue::Next() {
  DCHECK(!tasks_.empty());
  tasks_.erase(tasks_.begin());
}

bool InstallableTaskQueue::HasCurrent() const {
  return !tasks_.empty();
}
