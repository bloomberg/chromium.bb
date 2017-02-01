// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/task.h"

#include <stddef.h>

#include "base/process/process.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "content/public/common/result_codes.h"

namespace task_manager {

namespace {

// The last ID given to the previously created task.
int64_t g_last_id = 0;

}  // namespace

Task::Task(const base::string16& title,
           const std::string& rappor_sample,
           const gfx::ImageSkia* icon,
           base::ProcessHandle handle,
           base::ProcessId process_id)
    : task_id_(g_last_id++),
      network_usage_(-1),
      current_byte_count_(-1),
      title_(title),
      rappor_sample_name_(rappor_sample),
      icon_(icon ? *icon : gfx::ImageSkia()),
      process_handle_(handle),
      process_id_(process_id != base::kNullProcessId
                      ? process_id
                      : base::GetProcId(handle)) {}

Task::~Task() {}

// static
base::string16 Task::GetProfileNameFromProfile(Profile* profile) {
  DCHECK(profile);
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile->GetOriginalProfile()->GetPath(),
                                   &entry)) {
    return entry->GetName();
  }

  return base::string16();
}

void Task::Activate() {
}

bool Task::IsKillable() {
  return true;
}

void Task::Kill() {
  DCHECK_NE(process_id(), base::GetCurrentProcId());
  base::Process process = base::Process::Open(process_id());
  process.Terminate(content::RESULT_CODE_KILLED, false);
}

void Task::Refresh(const base::TimeDelta& update_interval,
                   int64_t refresh_flags) {
  if ((refresh_flags & REFRESH_TYPE_NETWORK_USAGE) == 0)
    return;

  if (current_byte_count_ == -1)
    return;

  network_usage_ =
      (current_byte_count_ * base::TimeDelta::FromSeconds(1)) / update_interval;

  // Reset the current byte count for this task.
  current_byte_count_ = 0;
}

void Task::OnNetworkBytesRead(int64_t bytes_read) {
  if (current_byte_count_ == -1)
    current_byte_count_ = 0;

  current_byte_count_ += bytes_read;
}

void Task::GetTerminationStatus(base::TerminationStatus* out_status,
                                int* out_error_code) const {
  DCHECK(out_status);
  DCHECK(out_error_code);

  *out_status = base::TERMINATION_STATUS_STILL_RUNNING;
  *out_error_code = 0;
}

base::string16 Task::GetProfileName() const {
  return base::string16();
}

int Task::GetTabId() const {
  return -1;
}

bool Task::HasParentTask() const {
  return GetParentTask() != nullptr;
}

const Task* Task::GetParentTask() const {
  return nullptr;
}

bool Task::ReportsSqliteMemory() const {
  return GetSqliteMemoryUsed() != -1;
}

int64_t Task::GetSqliteMemoryUsed() const {
  return -1;
}

bool Task::ReportsV8Memory() const {
  return GetV8MemoryAllocated() != -1;
}

int64_t Task::GetV8MemoryAllocated() const {
  return -1;
}

int64_t Task::GetV8MemoryUsed() const {
  return -1;
}

bool Task::ReportsWebCacheStats() const {
  return false;
}

blink::WebCache::ResourceTypeStats Task::GetWebCacheStats() const {
  return blink::WebCache::ResourceTypeStats();
}

int Task::GetKeepaliveCount() const {
  return -1;
}

bool Task::ReportsNetworkUsage() const {
  return network_usage_ != -1;
}

}  // namespace task_manager
