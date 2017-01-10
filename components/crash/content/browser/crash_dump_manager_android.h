// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_

#include <map>

#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "components/crash/content/browser/crash_dump_observer_android.h"

namespace breakpad {

// This class manages the crash minidumps.
// On Android, because of process isolation, each renderer process runs with a
// different UID. As a result, we cannot generate the minidumps in the browser
// (as the browser process does not have access to some system files for the
// crashed process). So the minidump is generated in the renderer process.
// Since the isolated process cannot open files, we provide it on creation with
// a file descriptor into which to write the minidump in the event of a crash.
// This class creates these file descriptors and associates them with render
// processes and takes the appropriate action when the render process
// terminates.
class CrashDumpManager : public breakpad::CrashDumpObserver::Client {
 public:
  CrashDumpManager(const base::FilePath& crash_dump_dir, int descriptor_id);
  ~CrashDumpManager() override;

  // breakpad::CrashDumpObserver::Client implementation:
  void OnChildStart(int child_process_id,
                    content::FileDescriptorInfo* mappings) override;
  void OnChildExit(int child_process_id,
                   base::ProcessHandle pid,
                   content::ProcessType process_type,
                   base::TerminationStatus termination_status,
                   base::android::ApplicationState app_state) override;

 private:
  typedef std::map<int, base::FilePath> ChildProcessIDToMinidumpPath;

  // This enum is used to back a UMA histogram, and must be treated as
  // append-only.
  enum ExitStatus {
    EMPTY_MINIDUMP_WHILE_RUNNING,
    EMPTY_MINIDUMP_WHILE_PAUSED,
    EMPTY_MINIDUMP_WHILE_BACKGROUND,
    VALID_MINIDUMP_WHILE_RUNNING,
    VALID_MINIDUMP_WHILE_PAUSED,
    VALID_MINIDUMP_WHILE_BACKGROUND,
    MINIDUMP_STATUS_COUNT
  };

  static void ProcessMinidump(const base::FilePath& minidump_path,
                              const base::FilePath& crash_dump_dir,
                              base::ProcessHandle pid,
                              content::ProcessType process_type,
                              base::TerminationStatus termination_status,
                              base::android::ApplicationState app_state);

  // This map should only be accessed with its lock aquired as it is accessed
  // from the PROCESS_LAUNCHER and UI threads.
  base::Lock child_process_id_to_minidump_path_lock_;
  ChildProcessIDToMinidumpPath child_process_id_to_minidump_path_;

  // The directory in which temporary minidump files should be created.
  base::FilePath crash_dump_dir_;

  // The id used to identify the file descriptor in the set of file
  // descriptor mappings passed to the child process.
  int descriptor_id_;

  DISALLOW_COPY_AND_ASSIGN(CrashDumpManager);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_
