// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "base/threading/non_thread_safe.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_LINUX) || defined(OS_OPENBSD)
#include "base/files/scoped_temp_dir.h"
#endif  // defined(OS_LINUX) || defined(OS_OPENBSD)

class CommandLine;

// ProcessSingleton ----------------------------------------------------------
//
// This class allows different browser processes to communicate with
// each other.  It is named according to the user data directory, so
// we can be sure that no more than one copy of the application can be
// running at once with a given data directory.
//
// Implementation notes:
// - the Windows implementation uses an invisible global message window;
// - the Linux implementation uses a Unix domain socket in the user data dir.

class ProcessSingleton : public base::NonThreadSafe {
 public:
  enum NotifyResult {
    PROCESS_NONE,
    PROCESS_NOTIFIED,
    PROFILE_IN_USE,
    LOCK_ERROR,
    NUM_NOTIFY_RESULTS,
  };

  // Implement this callback to handle notifications from other processes. The
  // callback will receive the command line and directory with which the other
  // Chrome process was launched. Return true if the command line will be
  // handled within the current browser instance or false if the remote process
  // should handle it (i.e., because the current process is shutting down).
  typedef base::Callback<bool(
      const CommandLine& command_line,
      const base::FilePath& current_directory)> NotificationCallback;

  explicit ProcessSingleton(const base::FilePath& user_data_dir);
  ~ProcessSingleton();

  // Notify another process, if available. Otherwise sets ourselves as the
  // singleton instance and stores the provided callback for notification from
  // future processes. Returns PROCESS_NONE if we became the singleton
  // instance. Callers are guaranteed to either have notified an existing
  // process or have grabbed the singleton (unless the profile is locked by an
  // unreachable process).
  // TODO(brettw): Make the implementation of this method non-platform-specific
  // by making Linux re-use the Windows implementation.
  NotifyResult NotifyOtherProcessOrCreate(
      const NotificationCallback& notification_callback);

  // Sets ourself up as the singleton instance.  Returns true on success.  If
  // false is returned, we are not the singleton instance and the caller must
  // exit. Otherwise, stores the provided callback for notification from
  // future processes.
  // NOTE: Most callers should generally prefer NotifyOtherProcessOrCreate() to
  // this method, only callers for whom failure is prefered to notifying another
  // process should call this directly.
  bool Create(const NotificationCallback& notification_callback);

  // Clear any lock state during shutdown.
  void Cleanup();

  // Blocks the dispatch of CopyData messages. foreground_window refers
  // to the window that should be set to the foreground if a CopyData message
  // is received while the ProcessSingleton is locked.
  void Lock(gfx::NativeWindow foreground_window) {
    DCHECK(CalledOnValidThread());
    locked_ = true;
    foreground_window_ = foreground_window;
  }

  // Changes the foreground window without changing the locked state.
  void SetForegroundWindow(gfx::NativeWindow foreground_window) {
    DCHECK(CalledOnValidThread());
    foreground_window_ = foreground_window;
  }

  // Allows the dispatch of CopyData messages and replays the messages which
  // were received when the ProcessSingleton was locked.
  void Unlock();

  bool locked() {
    DCHECK(CalledOnValidThread());
    return locked_;
  }

#if defined(OS_WIN)
  LRESULT WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  static void DisablePromptForTesting();
#endif  // defined(OS_LINUX) || defined(OS_OPENBSD)

 protected:
  // Notify another process, if available.
  // Returns true if another process was found and notified, false if we should
  // continue with the current process.
  // On Windows, Create() has to be called before this.
  NotifyResult NotifyOtherProcess();

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Exposed for testing.  We use a timeout on Linux, and in tests we want
  // this timeout to be short.
  NotifyResult NotifyOtherProcessWithTimeout(const CommandLine& command_line,
                                             int timeout_seconds,
                                             bool kill_unresponsive);
  NotifyResult NotifyOtherProcessWithTimeoutOrCreate(
      const CommandLine& command_line,
      const NotificationCallback& notification_callback,
      int timeout_seconds);
  void OverrideCurrentPidForTesting(base::ProcessId pid);
  void OverrideKillCallbackForTesting(
      const base::Callback<void(int)>& callback);
#endif  // defined(OS_LINUX) || defined(OS_OPENBSD)

 private:
  typedef std::pair<CommandLine::StringVector,
                    base::FilePath> DelayedStartupMessage;

#if !defined(OS_MACOSX)
  // Timeout for the current browser process to respond. 20 seconds should be
  // enough. It's only used in Windows and Linux implementations.
  static const int kTimeoutInSeconds = 20;
#endif

  bool locked_;
  gfx::NativeWindow foreground_window_;
  NotificationCallback notification_callback_;  // Handler for notifications.

#if defined(OS_WIN)
  // This ugly behemoth handles startup commands sent from another process.
  LRESULT OnCopyData(HWND hwnd, const COPYDATASTRUCT* cds);

  bool EscapeVirtualization(const base::FilePath& user_data_dir);

  HWND remote_window_;  // The HWND_MESSAGE of another browser.
  HWND window_;  // The HWND_MESSAGE window.
  bool is_virtualized_;  // Stuck inside Microsoft Softricity VM environment.
  HANDLE lock_file_;
  base::FilePath user_data_dir_;
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // Return true if the given pid is one of our child processes.
  // Assumes that the current pid is the root of all pids of the current
  // instance.
  bool IsSameChromeInstance(pid_t pid);

  // Extract the process's pid from a symbol link path and if it is on
  // the same host, kill the process, unlink the lock file and return true.
  // If the process is part of the same chrome instance, unlink the lock file
  // and return true without killing it.
  // If the process is on a different host, return false.
  bool KillProcessByLockPath();

  // Default function to kill a process, overridable by tests.
  void KillProcess(int pid);

  // Allow overriding for tests.
  base::ProcessId current_pid_;

  // Function to call when the other process is hung and needs to be killed.
  // Allows overriding for tests.
  base::Callback<void(int)> kill_callback_;

  // Path in file system to the socket.
  base::FilePath socket_path_;

  // Path in file system to the lock.
  base::FilePath lock_path_;

  // Path in file system to the cookie file.
  base::FilePath cookie_path_;

  // Temporary directory to hold the socket.
  base::ScopedTempDir socket_dir_;

  // Helper class for linux specific messages.  LinuxWatcher is ref counted
  // because it posts messages between threads.
  class LinuxWatcher;
  scoped_refptr<LinuxWatcher> watcher_;
#elif defined(OS_MACOSX)
  // Path in file system to the lock.
  base::FilePath lock_path_;

  // File descriptor associated with the lockfile, valid between
  // |Create()| and |Cleanup()|.  Two instances cannot have a lock on
  // the same file at the same time.
  int lock_fd_;
#endif

  // If messages are received in the locked state, the corresponding command
  // lines are saved here to be replayed later.
  std::vector<DelayedStartupMessage> saved_startup_messages_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSingleton);
};

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_H_
