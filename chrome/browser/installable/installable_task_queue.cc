// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_task_queue.h"

InstallableTaskQueue::InstallableTaskQueue() {}
InstallableTaskQueue::~InstallableTaskQueue() {}

void InstallableTaskQueue::Insert(InstallableTask task) {
  tasks_.push_back(task);
}

void InstallableTaskQueue::Reset() {
  tasks_.clear();
  paused_tasks_.clear();
}

bool InstallableTaskQueue::HasPaused() const {
  return !paused_tasks_.empty();
}

void InstallableTaskQueue::UnpauseAll() {
  for (const auto& task : paused_tasks_)
    Insert(task);

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

bool InstallableTaskQueue::IsEmpty() const {
  // TODO(mcgreevy): try to remove this method by removing the need to
  // explicitly call WorkOnTask.
  return tasks_.empty();
}
