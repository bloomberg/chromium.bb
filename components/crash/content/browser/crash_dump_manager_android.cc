// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_dump_manager_android.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/global_descriptors.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/file_descriptor_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace breakpad {

CrashDumpManager::CrashDumpManager(const base::FilePath& crash_dump_dir,
                                   int descriptor_id)
    : crash_dump_dir_(crash_dump_dir), descriptor_id_(descriptor_id) {}

CrashDumpManager::~CrashDumpManager() {
}

void CrashDumpManager::OnChildStart(int child_process_id,
                                    content::FileDescriptorInfo* mappings) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);

  if (!breakpad::IsCrashReporterEnabled())
    return;

  base::FilePath minidump_path;
  if (!base::CreateTemporaryFile(&minidump_path)) {
    LOG(ERROR) << "Failed to create temporary file, crash won't be reported.";
    return;
  }

  // We need read permission as the minidump is generated in several phases
  // and needs to be read at some point.
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE;
  base::File minidump_file(minidump_path, flags);
  if (!minidump_file.IsValid()) {
    LOG(ERROR) << "Failed to open temporary file, crash won't be reported.";
    return;
  }

  {
    base::AutoLock auto_lock(child_process_id_to_minidump_path_lock_);
    DCHECK(!base::ContainsKey(child_process_id_to_minidump_path_,
                              child_process_id));
    child_process_id_to_minidump_path_[child_process_id] = minidump_path;
  }
  mappings->Transfer(descriptor_id_,
                     base::ScopedFD(minidump_file.TakePlatformFile()));
}

// static
void CrashDumpManager::ProcessMinidump(
    const base::FilePath& minidump_path,
    const base::FilePath& crash_dump_dir,
    base::ProcessHandle pid,
    content::ProcessType process_type,
    base::TerminationStatus termination_status,
    base::android::ApplicationState app_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  int64_t file_size = 0;
  int r = base::GetFileSize(minidump_path, &file_size);
  DCHECK(r) << "Failed to retrieve size for minidump "
            << minidump_path.value();

  // TODO(wnwen): If these numbers match up to TabWebContentsObserver's
  //     TabRendererCrashStatus histogram, then remove that one as this is more
  //     accurate with more detail.
  if ((process_type == content::PROCESS_TYPE_RENDERER ||
       process_type == content::PROCESS_TYPE_GPU) &&
      app_state != base::android::APPLICATION_STATE_UNKNOWN) {
    ExitStatus exit_status;
    bool is_running =
        (app_state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    bool is_paused =
        (app_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES);
    if (file_size == 0) {
      if (is_running) {
        exit_status = EMPTY_MINIDUMP_WHILE_RUNNING;
      } else if (is_paused) {
        exit_status = EMPTY_MINIDUMP_WHILE_PAUSED;
      } else {
        exit_status = EMPTY_MINIDUMP_WHILE_BACKGROUND;
      }
    } else {
      if (is_running) {
        exit_status = VALID_MINIDUMP_WHILE_RUNNING;
      } else if (is_paused) {
        exit_status = VALID_MINIDUMP_WHILE_PAUSED;
      } else {
        exit_status = VALID_MINIDUMP_WHILE_BACKGROUND;
      }
    }
    if (process_type == content::PROCESS_TYPE_RENDERER) {
      if (termination_status == base::TERMINATION_STATUS_OOM_PROTECTED) {
        UMA_HISTOGRAM_ENUMERATION("Tab.RendererDetailedExitStatus",
                                  exit_status,
                                  ExitStatus::MINIDUMP_STATUS_COUNT);
      } else {
        UMA_HISTOGRAM_ENUMERATION("Tab.RendererDetailedExitStatusUnbound",
                                  exit_status,
                                  ExitStatus::MINIDUMP_STATUS_COUNT);
      }
    } else if (process_type == content::PROCESS_TYPE_GPU) {
      UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessDetailedExitStatus",
                                exit_status,
                                ExitStatus::MINIDUMP_STATUS_COUNT);
    }
  }

  if (file_size == 0) {
    // Empty minidump, this process did not crash. Just remove the file.
    r = base::DeleteFile(minidump_path, false);
    DCHECK(r) << "Failed to delete temporary minidump file "
              << minidump_path.value();
    return;
  }

  // We are dealing with a valid minidump. Copy it to the crash report
  // directory from where Java code will upload it later on.
  if (crash_dump_dir.empty()) {
    NOTREACHED() << "Failed to retrieve the crash dump directory.";
    return;
  }
  const uint64_t rand = base::RandUint64();
  const std::string filename =
      base::StringPrintf("chromium-renderer-minidump-%016" PRIx64 ".dmp%d",
                         rand, pid);
  base::FilePath dest_path = crash_dump_dir.Append(filename);
  r = base::Move(minidump_path, dest_path);
  if (!r) {
    LOG(ERROR) << "Failed to move crash dump from " << minidump_path.value()
               << " to " << dest_path.value();
    base::DeleteFile(minidump_path, false);
    return;
  }
  VLOG(1) << "Crash minidump successfully generated: "
          << crash_dump_dir.Append(filename).value();
}

void CrashDumpManager::OnChildExit(int child_process_id,
                                   base::ProcessHandle pid,
                                   content::ProcessType process_type,
                                   base::TerminationStatus termination_status,
                                   base::android::ApplicationState app_state) {
  base::FilePath minidump_path;
  {
    base::AutoLock auto_lock(child_process_id_to_minidump_path_lock_);
    ChildProcessIDToMinidumpPath::iterator iter =
        child_process_id_to_minidump_path_.find(child_process_id);
    if (iter == child_process_id_to_minidump_path_.end()) {
      // We might get a NOTIFICATION_RENDERER_PROCESS_TERMINATED and a
      // NOTIFICATION_RENDERER_PROCESS_CLOSED.
      return;
    }
    minidump_path = iter->second;
    child_process_id_to_minidump_path_.erase(iter);
  }
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&CrashDumpManager::ProcessMinidump, minidump_path,
                 crash_dump_dir_, pid, process_type, termination_status,
                 app_state));
}

}  // namespace breakpad
