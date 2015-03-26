// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_BROWSER_WATCHER_WINDOW_HANG_MONITOR_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_WINDOW_HANG_MONITOR_WIN_H_

#include <windows.h>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace browser_watcher {

// Monitors a window for hanging by periodically sending it a WM_NULL message
// and timing the response.
class WindowHangMonitor {
 public:
  enum WindowEvent {
    WINDOW_HUNG,
    WINDOW_VANISHED,
  };
  // Called when a hang is detected or when the window has gone away.
  // Called precisely zero or one time(s).
  typedef base::Callback<void(WindowEvent)> WindowEventCallback;

  // Initialize the monitor with an event callback.
  explicit WindowHangMonitor(const WindowEventCallback& callback);
  ~WindowHangMonitor();

  // Initializes the watcher to monitor the window answering to |window_name|.
  // Returns true on success.
  bool Initialize(const base::string16& window_name);

  // Testing accessors.
  bool IsIdleForTesting() const { return !timer_.IsRunning(); }
  void SetPingIntervalForTesting(base::TimeDelta ping_interval);
  void SetHangTimeoutForTesting(base::TimeDelta hang_timeout);

  HWND window() const { return window_; }
  const base::Process& window_process() const { return window_process_; }

 private:
  struct OutstandingPing {
    WindowHangMonitor* monitor;
  };

  static void CALLBACK
  OnPongReceived(HWND window, UINT msg, ULONG_PTR data, LRESULT lresult);

  // Checks that |window_| is still valid, and sends it a ping.
  // Issues a |WINDOW_VANISHED| callback if the window's no longer valid.
  // Schedules OnHangTimeout in case of success.
  // Returns true on success, false if the window is no longer valid or other
  // failure.
  bool MaybeSendPing();

  // Runs after a |hang_timeout_| delay after sending a ping. Checks whether
  // a pong was received. Either issues a callback or schedules OnRetryTimeout.
  void OnHangTimeout();

  // Runs periodically at |ping_interval_| interval, as long as the window is
  // still valid and not hung.
  void OnRetryTimeout();

  // Invoked on significant window events.
  WindowEventCallback callback_;

  // The name of the (message) window to monitor.
  base::string16 window_name_;

  // The monitored window handle.
  HWND window_;

  // The process that owned |window_| when Initialize was called.
  base::Process window_process_;

  // The time the last message was sent.
  base::Time last_ping_;

  // The ping interval, must be larger than |hang_timeout_|.
  base::TimeDelta ping_interval_;

  // The time after which |window_| is assumed hung.
  base::TimeDelta hang_timeout_;

  // The timer used to schedule polls.
  base::Timer timer_;

  // Non-null when there is an outstanding ping.
  // This is intentionally leaked when a hang is detected.
  OutstandingPing* outstanding_ping_;

  DISALLOW_COPY_AND_ASSIGN(WindowHangMonitor);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_WINDOW_HANG_MONITOR_WIN_H_
