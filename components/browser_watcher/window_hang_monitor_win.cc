// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/browser_watcher/window_hang_monitor_win.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/win/message_window.h"

namespace browser_watcher {

namespace {

const size_t kPingIntervalSeconds = 60;
const size_t kHangTimeoutSeconds = 20;

bool IsWindowValid(HWND window,
                   const base::string16& window_name,
                   base::ProcessId pid) {
  // Validate the Window in two respects:
  // 1. The window handle might have been re-assigned to a different window
  //    from the time we found it to the point where we query for its owning
  //    process.
  // 2. The window handle might have been re-assigned to a different process
  //    at any point after we found it.
  if (window != base::win::MessageWindow::FindWindow(window_name)) {
    // The window handle has been reassigned, bail.
    return false;
  }

  // Re-do the process ID lookup.
  DWORD new_process_id = 0;
  DWORD thread_id = ::GetWindowThreadProcessId(window, &new_process_id);
  if (thread_id == 0 || pid != new_process_id) {
    // Another process has taken over the handle.
    return false;
  }

  return true;
}

}  // namespace

WindowHangMonitor::WindowHangMonitor(const WindowEventCallback& callback)
    : callback_(callback),
      window_(NULL),
      outstanding_ping_(nullptr),
      timer_(false /* don't retain user task */, false /* don't repeat */),
      ping_interval_(base::TimeDelta::FromSeconds(kPingIntervalSeconds)),
      hang_timeout_(base::TimeDelta::FromSeconds(kHangTimeoutSeconds)) {
}

WindowHangMonitor::~WindowHangMonitor() {
  if (outstanding_ping_) {
    // We have an outstanding ping, disable it and leak it intentionally as
    // if the callback arrives eventually, it'll cause a use-after-free.
    outstanding_ping_->monitor = nullptr;
    outstanding_ping_ = nullptr;
  }
}

bool WindowHangMonitor::Initialize(const base::string16& window_name) {
  window_name_ = window_name;
  timer_.SetTaskRunner(base::MessageLoop::current()->task_runner());

  // This code is fraught with all kinds of races. As the purpose here is
  // only monitoring, this code simply bails if any kind of race is encountered.
  // Find the window to monitor by name.
  window_ = base::win::MessageWindow::FindWindow(window_name);
  if (window_ == NULL)
    return false;

  // Find and open the process owning this window.
  DWORD process_id = 0;
  DWORD thread_id = ::GetWindowThreadProcessId(window_, &process_id);
  if (thread_id == 0 || process_id == 0) {
    // The window has vanished or there was some other problem with the handle.
    return false;
  }

  // Keep an open handle on the process to make sure the PID isn't reused.
  window_process_ = base::Process::Open(process_id);
  if (!window_process_.IsValid()) {
    // The process may be at a different security level.
    return false;
  }

  return MaybeSendPing();
}

void WindowHangMonitor::SetPingIntervalForTesting(
    base::TimeDelta ping_interval) {
  ping_interval_ = ping_interval;
}

void WindowHangMonitor::SetHangTimeoutForTesting(base::TimeDelta hang_timeout) {
  hang_timeout_ = hang_timeout;
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

bool WindowHangMonitor::MaybeSendPing() {
  DCHECK(window_process_.IsValid());
  DCHECK(window_);
  DCHECK(!outstanding_ping_);

  if (!IsWindowValid(window_, window_name_, window_process_.Pid())) {
    // The window is no longer valid, issue the callback.
    callback_.Run(WINDOW_VANISHED);
    return false;
  }

  // The window checks out, issue a ping against it. Set up all state ahead of
  // time to allow for the possibility of the callback being invoked from within
  // SendMessageCallback.
  outstanding_ping_ = new OutstandingPing;
  outstanding_ping_->monitor = this;

  // Note that this is still racy to |window_| having been re-assigned, but
  // the race is as small as we can make it, and the next attempt will re-try.
  if (!::SendMessageCallback(window_, WM_NULL, 0, 0, &OnPongReceived,
                             reinterpret_cast<ULONG_PTR>(outstanding_ping_))) {
    // Message sending failed, assume the window is no longer valid,
    // issue the callback and stop the polling.
    delete outstanding_ping_;
    outstanding_ping_ = nullptr;

    callback_.Run(WINDOW_VANISHED);
    return false;
  }

  // Issue the count-out callback.
  timer_.Start(
      FROM_HERE, hang_timeout_,
      base::Bind(&WindowHangMonitor::OnHangTimeout, base::Unretained(this)));

  return true;
}

void WindowHangMonitor::OnHangTimeout() {
  DCHECK(window_process_.IsValid());
  DCHECK(window_);

  if (outstanding_ping_) {
    // The ping is still outstanding, the window is hung or has vanished.
    // Orphan the outstanding ping. If the callback arrives late, it will
    // delete it, or if the callback never arrives it'll leak.
    outstanding_ping_->monitor = NULL;
    outstanding_ping_ = NULL;

    if (!IsWindowValid(window_, window_name_, window_process_.Pid())) {
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
  MaybeSendPing();
}

}  // namespace browser_watcher
