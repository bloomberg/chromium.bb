// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_POSIX)
#include "base/file_path.h"
#endif  // defined(OS_POSIX)

#if defined(USE_X11)
#include "base/scoped_temp_dir.h"
#endif  // defined(USE_X11)

class CommandLine;
class FilePath;

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
  };

  explicit ProcessSingleton(const FilePath& user_data_dir);
  ~ProcessSingleton();

  // Notify another process, if available.
  // Returns true if another process was found and notified, false if we
  // should continue with this process.
  // Windows code roughly based on Mozilla.
  //
  // TODO(brettw): this will not handle all cases. If two process start up too
  // close to each other, the Create() might not yet have happened for the
  // first one, so this function won't find it.
  NotifyResult NotifyOtherProcess();

  // Notify another process, if available.  Otherwise sets ourselves as the
  // singleton instance.  Returns PROCESS_NONE if we became the singleton
  // instance.
  NotifyResult NotifyOtherProcessOrCreate();

#if defined(OS_LINUX)
  // Exposed for testing.  We use a timeout on Linux, and in tests we want
  // this timeout to be short.
  NotifyResult NotifyOtherProcessWithTimeout(const CommandLine& command_line,
                                             int timeout_seconds,
                                             bool kill_unresponsive);
  NotifyResult NotifyOtherProcessWithTimeoutOrCreate(
      const CommandLine& command_line,
      int timeout_seconds);
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
  // Used in specific cases to let us know that there is an existing instance
  // of Chrome running with this profile. In general, you should not use this
  // function. Instead consider using NotifyOtherProcessOrCreate().
  // For non profile-specific method, use Upgrade::IsBrowserAlreadyRunning().
  bool FoundOtherProcessWindow() const {
      return (NULL != remote_window_);
  }
#endif  // defined(OS_WIN)

  // Sets ourself up as the singleton instance.  Returns true on success.  If
  // false is returned, we are not the singleton instance and the caller must
  // exit.
  bool Create();

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

  // Allows the dispatch of CopyData messages.
  void Unlock() {
    DCHECK(CalledOnValidThread());
    locked_ = false;
    foreground_window_ = NULL;
  }

  bool locked() {
    DCHECK(CalledOnValidThread());
    return locked_;
  }

 private:
#if !defined(OS_MACOSX)
  // Timeout for the current browser process to respond. 20 seconds should be
  // enough. It's only used in Windows and Linux implementations.
  static const int kTimeoutInSeconds = 20;
#endif

  bool locked_;
  gfx::NativeWindow foreground_window_;

#if defined(OS_WIN)
  // This ugly behemoth handles startup commands sent from another process.
  LRESULT OnCopyData(HWND hwnd, const COPYDATASTRUCT* cds);

  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);

  static LRESULT CALLBACK WndProcStatic(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
    ProcessSingleton* msg_wnd = reinterpret_cast<ProcessSingleton*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  }

  HWND remote_window_;  // The HWND_MESSAGE of another browser.
  HWND window_;  // The HWND_MESSAGE window.
#elif defined(USE_X11)
  // Path in file system to the socket.
  FilePath socket_path_;

  // Path in file system to the lock.
  FilePath lock_path_;

  // Path in file system to the cookie file.
  FilePath cookie_path_;

  // Temporary directory to hold the socket.
  ScopedTempDir socket_dir_;

  // Helper class for linux specific messages.  LinuxWatcher is ref counted
  // because it posts messages between threads.
  class LinuxWatcher;
  scoped_refptr<LinuxWatcher> watcher_;
#elif defined(OS_MACOSX)
  // Path in file system to the lock.
  FilePath lock_path_;

  // File descriptor associated with the lockfile, valid between
  // |Create()| and |Cleanup()|.  Two instances cannot have a lock on
  // the same file at the same time.
  int lock_fd_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProcessSingleton);
};

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_H_
