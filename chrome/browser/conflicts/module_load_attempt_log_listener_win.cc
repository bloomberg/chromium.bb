// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_load_attempt_log_listener_win.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/conflicts/module_blacklist_cache_util_win.h"
#include "chrome_elf/third_party_dlls/logging_api.h"
#include "chrome_elf/third_party_dlls/packed_list_format.h"

namespace {

std::vector<third_party_dlls::PackedListModule> DrainLogOnBackgroundTask() {
  // Query the number of bytes needed.
  uint32_t bytes_needed = 0;
  DrainLog(nullptr, 0, &bytes_needed);

  // Drain the log.
  auto buffer = std::make_unique<uint8_t[]>(bytes_needed);
  uint32_t bytes_written = DrainLog(buffer.get(), bytes_needed, nullptr);
  DCHECK_EQ(bytes_needed, bytes_written);

  auto now_time_date_stamp = CalculateTimeDateStamp(base::Time::Now());

  // Parse the data using the recommanded pattern for iterating over the log.
  std::vector<third_party_dlls::PackedListModule> blocked_modules;
  uint8_t* tracker = buffer.get();
  uint8_t* buffer_end = buffer.get() + bytes_written;
  while (tracker < buffer_end) {
    third_party_dlls::LogEntry* entry =
        reinterpret_cast<third_party_dlls::LogEntry*>(tracker);
    DCHECK_LE(tracker + third_party_dlls::GetLogEntrySize(entry->path_len),
              buffer_end);

    // Only consider blocked modules.
    // TODO(pmonette): Wire-up loaded modules to ModuleDatabase::OnModuleLoad to
    // get better visibility into all modules that loads into the browser
    // process.
    if (entry->type == third_party_dlls::LogType::kBlocked) {
      // No log path should be empty.
      DCHECK(entry->path_len);
      blocked_modules.emplace_back();
      third_party_dlls::PackedListModule& module = blocked_modules.back();
      // Fill in a PackedListModule from the log entry.
      std::string hash_string =
          base::SHA1HashString(third_party_dlls::GetFingerprintString(
              entry->time_date_stamp, entry->module_size));
      std::copy(std::begin(hash_string), std::end(hash_string),
                std::begin(module.code_id_hash));
      // |entry->path| is a UTF-8 device path.  A hash of the
      // lowercase, UTF-8 basename is needed for |module.basename_hash|.
      base::FilePath file_path(base::UTF8ToUTF16(entry->path));
      std::wstring basename = base::i18n::ToLower(file_path.BaseName().value());
      hash_string = base::UTF16ToUTF8(basename);
      hash_string = base::SHA1HashString(hash_string);
      std::copy(std::begin(hash_string), std::end(hash_string),
                std::begin(module.basename_hash));
      module.time_date_stamp = now_time_date_stamp;

      // TODO(pmonette): |file_path| is ready for
      //                 base::DevicePathToDriveLetterPath() here if needed.
      //                 Consider making these path conversions more efficient
      //                 by caching the local mounted devices and corresponding
      //                 drive paths once.
    }

    tracker += third_party_dlls::GetLogEntrySize(entry->path_len);
  }

  return blocked_modules;
}

}  // namespace

ModuleLoadAttemptLogListener::ModuleLoadAttemptLogListener(
    OnNewModulesBlockedCallback on_new_modules_blocked_callback)
    : on_new_modules_blocked_callback_(
          std::move(on_new_modules_blocked_callback)),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
           base::MayBlock()})),
      // The event starts signaled so that the logs are drained once when the
      // |object_watcher_| starts waiting on the newly registered event.
      waitable_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::SIGNALED),
      weak_ptr_factory_(this) {
  if (!RegisterLogNotification(waitable_event_.handle()))
    return;

  object_watcher_.StartWatchingMultipleTimes(waitable_event_.handle(), this);
}

ModuleLoadAttemptLogListener::~ModuleLoadAttemptLogListener() = default;

void ModuleLoadAttemptLogListener::OnObjectSignaled(HANDLE object) {
  StartDrainingLogs();
}

void ModuleLoadAttemptLogListener::StartDrainingLogs() {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&DrainLogOnBackgroundTask),
      base::BindOnce(&ModuleLoadAttemptLogListener::OnLogDrained,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ModuleLoadAttemptLogListener::OnLogDrained(
    std::vector<third_party_dlls::PackedListModule>&& blocked_modules) {
  on_new_modules_blocked_callback_.Run(std::move(blocked_modules));
}
