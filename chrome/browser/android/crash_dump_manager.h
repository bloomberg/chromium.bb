// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CRASH_DUMP_MANAGER_H_
#define CHROME_BROWSER_ANDROID_CRASH_DUMP_MANAGER_H_

#include <map>

#include "base/files/file_path.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class RenderProcessHost;
}

// This class manages the crash minidumps.
// On Android, because of process isolation, each renderer process runs with a
// different UID. As a result, we cannot generate the minidumps in the browser
// (as the browser process does not have access to some system files for the
// crashed process). So the minidump is generated in the renderer process.
// Since the isolated process cannot open files, we provide it on creation with
// a file descriptor where to write the minidump in the event of a crash.
// This class creates these file descriptors and associates them with render
// processes and take the appropriate action when the render process terminates.
class CrashDumpManager : public content::BrowserChildProcessObserver,
                         public content::NotificationObserver {
 public:
  // This object is a singleton created and owned by the
  // ChromeBrowserMainPartsAndroid.
  static CrashDumpManager* GetInstance();

  virtual ~CrashDumpManager();

  // Returns a file descriptor that should be used to generate a minidump for
  // the process |child_process_id|.
  int CreateMinidumpFile(int child_process_id);

 private:
  friend class ChromeBrowserMainPartsAndroid;

  // Should be created on the UI thread.
  CrashDumpManager();

  typedef std::map<int, base::FilePath> ChildProcessIDToMinidumpPath;

  static void ProcessMinidump(const base::FilePath& minidump_path,
                              base::ProcessHandle pid);

  // content::BrowserChildProcessObserver implementation:
  virtual void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessCrashed(
      const content::ChildProcessData& data) OVERRIDE;

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Called on child process exit (including crash).
  void OnChildExit(int child_process_id, base::ProcessHandle pid);

  content::NotificationRegistrar notification_registrar_;

  // This map should only be accessed with its lock aquired as it is accessed
  // from the PROCESS_LAUNCHER and UI threads.
  base::Lock child_process_id_to_minidump_path_lock_;
  ChildProcessIDToMinidumpPath child_process_id_to_minidump_path_;

  static CrashDumpManager* instance_;

  DISALLOW_COPY_AND_ASSIGN(CrashDumpManager);
};

#endif  // CHROME_BROWSER_ANDROID_CRASH_DUMP_MANAGER_H_
