// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/browser_watcher/window_hang_monitor_win.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/win/message_window.h"

namespace browser_watcher {

namespace {

HWND FindNamedWindowForProcess(const base::string16 name, base::ProcessId pid) {
  HWND candidate = base::win::MessageWindow::FindWindow(name);
  if (candidate) {
    DWORD actual_process_id = 0;
    ::GetWindowThreadProcessId(candidate, &actual_process_id);
    if (actual_process_id == pid)
      return candidate;
  }
  return nullptr;
}

}  // namespace

WindowHangMonitor::WindowHangMonitor(base::TimeDelta ping_interval,
                                     base::TimeDelta timeout,
                                     const WindowEventCallback& callback)
    : callback_(callback),
      ping_interval_(ping_interval),
      hang_timeout_(timeout),
      timer_(false /* don't retain user task */, false /* don't repeat */),
      outstanding_ping_(nullptr) {
}

WindowHangMonitor::~WindowHangMonitor() {
  if (outstanding_ping_) {
    // We have an outstanding ping, disable it and leak it intentionally as
    // if the callback arrives eventually, it'll cause a use-after-free.
    outstanding_ping_->monitor = nullptr;
    outstanding_ping_ = nullptr;
  }
}

void WindowHangMonitor::Initialize(base::Process process,
                                   const base::string16& window_name) {
  window_name_ = window_name;
  window_process_ = process.Pass();
  timer_.SetTaskRunner(base::MessageLoop::current()->task_runner());

  ScheduleFindWindow();
}

void WindowHangMonitor::ScheduleFindWindow() {
  // TODO(erikwright): We could reduce the polling by using WaitForInputIdle,
  // but it is hard to test (requiring a non-Console executable).
  timer_.Start(
      FROM_HERE, ping_interval_,
      base::Bind(&WindowHangMonitor::PollForWindow, base::Unretained(this)));
}

void WindowHangMonitor::PollForWindow() {
  int exit_code = 0;
  if (window_process_.WaitForExitWithTimeout(base::TimeDelta(), &exit_code)) {
    callback_.Run(WINDOW_NOT_FOUND);
    return;
  }

  HWND hwnd = FindNamedWindowForProcess(window_name_, window_process_.Pid());
  if (hwnd) {
    // Sends a ping and schedules a timeout task. Upon receiving a ping response
    // further pings will be scheduled ad infinitum. Will signal any failure now
    // or later via the callback.
    SendPing(hwnd);
  } else {
    ScheduleFindWindow();
  }
}

void CALLBACK WindowHangMonitor::OnPongReceived(HWND window,
                                                UINT msg,
                                                ULONG_PTR data,
                                                LRESULT lresult) {
  OutstandingPing* outstanding = reinterpret_cast<OutstandingPing*>(data);

  // If the monitor is still around, clear its pointer.
  if (outstanding->monitor)
    outstanding->monitor->outstanding_ping_ = nullptr;

  delete outstanding;
}

void WindowHangMonitor::SendPing(HWND hwnd) {
  // Set up all state ahead of time to allow for the possibility of the callback
  // being invoked from within SendMessageCallback.
  outstanding_ping_ = new OutstandingPing;
  outstanding_ping_->monitor = this;

  // Note that this is racy to |hwnd| having been re-assigned. If that occurs,
  // we might fail to identify the disappearance of the window with this ping.
  // This is acceptable, as the next ping should detect it.
  if (!::SendMessageCallback(hwnd, WM_NULL, 0, 0, &OnPongReceived,
                             reinterpret_cast<ULONG_PTR>(outstanding_ping_))) {
    // Message sending failed, assume the window is no longer valid,
    // issue the callback and stop the polling.
    delete outstanding_ping_;
    outstanding_ping_ = nullptr;

    callback_.Run(WINDOW_VANISHED);
    return;
  }

  // Issue the count-out callback.
  timer_.Start(FROM_HERE, hang_timeout_,
               base::Bind(&WindowHangMonitor::OnHangTimeout,
                          base::Unretained(this), hwnd));
}

void WindowHangMonitor::OnHangTimeout(HWND hwnd) {
  DCHECK(window_process_.IsValid());

  if (outstanding_ping_) {
    // The ping is still outstanding, the window is hung or has vanished.
    // Orphan the outstanding ping. If the callback arrives late, it will
    // delete it, or if the callback never arrives it'll leak.
    outstanding_ping_->monitor = NULL;
    outstanding_ping_ = NULL;

    if (hwnd !=
        FindNamedWindowForProcess(window_name_, window_process_.Pid())) {
      // The window vanished.
      callback_.Run(WINDOW_VANISHED);
    } else {
      // The window hung.
      callback_.Run(WINDOW_HUNG);
    }
  } else {
    // No ping outstanding, window is not yet hung. Schedule the next retry.
    timer_.Start(
        FROM_HERE, hang_timeout_ - ping_interval_,
        base::Bind(&WindowHangMonitor::OnRetryTimeout, base::Unretained(this)));
  }
}

void WindowHangMonitor::OnRetryTimeout() {
  DCHECK(window_process_.IsValid());
  DCHECK(!outstanding_ping_);
  // We can't simply hold onto the previously located HWND due to potential
  // aliasing.
  // 1. The window handle might have been re-assigned to a different window
  //    from the time we found it to the point where we query for its owning
  //    process.
  // 2. The window handle might have been re-assigned to a different process
  //    at any point after we found it.
  HWND hwnd = FindNamedWindowForProcess(window_name_, window_process_.Pid());
  if (hwnd)
    SendPing(hwnd);
  else
    callback_.Run(WINDOW_VANISHED);
}

}  // namespace browser_watcher
