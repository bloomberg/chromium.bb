// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/task.h"

#include "chrome/browser/task_management/task_manager_observer.h"

namespace task_management {

namespace {

// The last ID given to the previously created task.
int64 g_last_id = 0;

}  // namespace


Task::Task(const base::string16& title,
           const gfx::ImageSkia* icon,
           base::ProcessHandle handle)
    : task_id_(g_last_id++),
      network_usage_(-1),
      current_byte_count_(-1),
      title_(title),
      icon_(icon ? *icon : gfx::ImageSkia()),
      process_handle_(handle),
      process_id_(base::GetProcId(handle)) {
}

Task::~Task() {
}

void Task::Activate() {
}

void Task::Refresh(const base::TimeDelta& update_interval,
                   int64 refresh_flags) {
  if ((refresh_flags & REFRESH_TYPE_NETWORK_USAGE) == 0)
    return;

  if (current_byte_count_ == -1)
    return;

  network_usage_ =
      (current_byte_count_ * base::TimeDelta::FromSeconds(1)) / update_interval;

  // Reset the current byte count for this task.
  current_byte_count_ = 0;
}

void Task::OnNetworkBytesRead(int64 bytes_read) {
  if (current_byte_count_ == -1)
    current_byte_count_ = 0;

  current_byte_count_ += bytes_read;
}

base::string16 Task::GetProfileName() const {
  return base::string16();
}

bool Task::ReportsSqliteMemory() const {
  return GetSqliteMemoryUsed() != -1;
}

int64 Task::GetSqliteMemoryUsed() const {
  return -1;
}

bool Task::ReportsV8Memory() const {
  return GetV8MemoryAllocated() != -1;
}

int64 Task::GetV8MemoryAllocated() const {
  return -1;
}

int64 Task::GetV8MemoryUsed() const {
  return -1;
}

bool Task::ReportsWebCacheStats() const {
  return false;
}

blink::WebCache::ResourceTypeStats Task::GetWebCacheStats() const {
  return blink::WebCache::ResourceTypeStats();
}

bool Task::ReportsNetworkUsage() const {
  return network_usage_ != -1;
}

}  // namespace task_management
