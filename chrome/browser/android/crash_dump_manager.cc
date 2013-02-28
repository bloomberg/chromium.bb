// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/crash_dump_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "base/process.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/descriptors_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/file_descriptor_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

// static
CrashDumpManager* CrashDumpManager::instance_ = NULL;

// static
CrashDumpManager* CrashDumpManager::GetInstance() {
  return instance_;
}

CrashDumpManager::CrashDumpManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!instance_);

  instance_ = this;

  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              content::NotificationService::AllSources());

  BrowserChildProcessObserver::Add(this);
}

CrashDumpManager::~CrashDumpManager() {
  instance_ = NULL;

  BrowserChildProcessObserver::Remove(this);
}

int CrashDumpManager::CreateMinidumpFile(int child_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER));
  base::FilePath minidump_path;
  if (!file_util::CreateTemporaryFile(&minidump_path))
    return base::kInvalidPlatformFileValue;

  base::PlatformFileError error;
  // We need read permission as the minidump is generated in several phases
  // and needs to be read at some point.
  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_WRITE;
  base::PlatformFile minidump_file =
      base::CreatePlatformFile(minidump_path, flags, NULL, &error);
  if (minidump_file == base::kInvalidPlatformFileValue) {
    LOG(ERROR) << "Failed to create temporary file, crash won't be reported.";
    return base::kInvalidPlatformFileValue;
  }

  {
    base::AutoLock auto_lock(child_process_id_to_minidump_path_lock_);
    DCHECK(!ContainsKey(child_process_id_to_minidump_path_, child_process_id));
    child_process_id_to_minidump_path_[child_process_id] = minidump_path;
  }
  return minidump_file;
}

void CrashDumpManager::ProcessMinidump(const base::FilePath& minidump_path,
                                       base::ProcessHandle pid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  int64 file_size = 0;
  int r = file_util::GetFileSize(minidump_path, &file_size);
  DCHECK(r) << "Failed to retrieve size for minidump "
            << minidump_path.value();

  if (file_size == 0) {
    // Empty minidump, this process did not crash. Just remove the file.
    r = file_util::Delete(minidump_path, false);
    DCHECK(r) << "Failed to delete temporary minidump file "
              << minidump_path.value();
    return;
  }

  // We are dealing with a valid minidump. Copy it to the crash report
  // directory from where Java code will upload it later on.
  base::FilePath crash_dump_dir;
  r = PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_dir);
  if (!r) {
    NOTREACHED() << "Failed to retrieve the crash dump directory.";
    return;
  }

  const uint64 rand = base::RandUint64();
  const std::string filename =
      base::StringPrintf("chromium-renderer-minidump-%016" PRIx64 ".dmp%d",
                         rand, pid);
  base::FilePath dest_path = crash_dump_dir.Append(filename);
  r = file_util::Move(minidump_path, dest_path);
  if (!r) {
    LOG(ERROR) << "Failed to move crash dump from " << minidump_path.value()
               << " to " << dest_path.value();
    file_util::Delete(minidump_path, false);
    return;
  }
  LOG(INFO) << "Crash minidump successfully generated: " <<
      crash_dump_dir.Append(filename).value();
}

void CrashDumpManager::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  OnChildExit(data.id, data.handle);
}

void CrashDumpManager::BrowserChildProcessCrashed(
    const content::ChildProcessData& data) {
  OnChildExit(data.id, data.handle);
}

void CrashDumpManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
      // NOTIFICATION_RENDERER_PROCESS_TERMINATED is sent when the renderer
      // process is cleanly shutdown. However, we need to fallthrough so that
      // we close the minidump_fd we kept open.
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnChildExit(rph->GetID(), rph->GetHandle());
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

void CrashDumpManager::OnChildExit(int child_process_id,
                                   base::ProcessHandle pid) {
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
      base::Bind(&CrashDumpManager::ProcessMinidump, minidump_path, pid));
}
