// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/window_hang_monitor_win.h"

#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/thread.h"
#include "base/win/message_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace browser_watcher {

namespace {

// Simulates a process that never opens a window.
MULTIPROCESS_TEST_MAIN(NoWindowChild) {
  ::Sleep(INFINITE);
  return 0;
}

// Manages a WindowHangMonitor that lives on a background thread.
class HangMonitorThread {
 public:
  // Instantiates the background thread.
  HangMonitorThread()
      : event_(WindowHangMonitor::WINDOW_NOT_FOUND),
        event_received_(false, false),
        thread_("HangMonitorThread") {}

  ~HangMonitorThread() {
    if (hang_monitor_.get())
      DestroyWatcher();
  }

  // Starts the background thread and the monitor to observe the window named
  // |window_name| in |process|. Blocks until the monitor has been initialized.
  bool Start(base::Process process, const base::string16& window_name) {
    if (!thread_.StartWithOptions(
            base::Thread::Options(base::MessageLoop::TYPE_UI, 0))) {
      return false;
    }

    base::WaitableEvent complete(false, false);
    if (!thread_.task_runner()->PostTask(
            FROM_HERE, base::Bind(&HangMonitorThread::StartupOnThread,
                                  base::Unretained(this), window_name,
                                  base::Passed(process.Pass()),
                                  base::Unretained(&complete)))) {
      return false;
    }

    complete.Wait();

    return true;
  }

  // Returns true if a window event is detected within |timeout|.
  bool TimedWaitForEvent(base::TimeDelta timeout) {
    return event_received_.TimedWait(timeout);
  }

  // Blocks indefinitely for a window event and returns it.
  WindowHangMonitor::WindowEvent WaitForEvent() {
    event_received_.Wait();
    return event_;
  }

  // Destroys the monitor and stops the background thread. Blocks until the
  // operation completes.
  void DestroyWatcher() {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&HangMonitorThread::ShutdownOnThread,
                              base::Unretained(this)));
    // This will block until the above-posted task completes.
    thread_.Stop();
  }

 private:
  // Invoked when the monitor signals an event. Unblocks a call to
  // TimedWaitForEvent or WaitForEvent.
  void EventCallback(WindowHangMonitor::WindowEvent event) {
    if (event_received_.IsSignaled())
      ADD_FAILURE() << "Multiple calls to EventCallback.";
    event_ = event;
    event_received_.Signal();
  }

  // Initializes the WindowHangMonitor to observe the window named |window_name|
  // in |process|. Signals |complete| when done.
  void StartupOnThread(const base::string16& window_name,
                       base::Process process,
                       base::WaitableEvent* complete) {
    hang_monitor_.reset(new WindowHangMonitor(
        base::TimeDelta::FromMilliseconds(100),
        base::TimeDelta::FromMilliseconds(100),
        base::Bind(&HangMonitorThread::EventCallback, base::Unretained(this))));
    hang_monitor_->Initialize(process.Pass(), window_name);
    complete->Signal();
  }

  // Destroys the WindowHangMonitor.
  void ShutdownOnThread() { hang_monitor_.reset(); }

  // The detected event. Invalid if |event_received_| has not been signaled.
  WindowHangMonitor::WindowEvent event_;
  // Indicates that |event_| has been assigned in response to a callback from
  // the WindowHangMonitor.
  base::WaitableEvent event_received_;
  // The WindowHangMonitor under test.
  scoped_ptr<WindowHangMonitor> hang_monitor_;
  // The background thread.
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(HangMonitorThread);
};

class WindowHangMonitorTest : public testing::Test {
 public:
  WindowHangMonitorTest()
      : ping_event_(false, false),
        pings_(0),
        window_thread_("WindowHangMonitorTest window_thread") {}

  void SetUp() override {
    // Pick a window name unique to this process.
    window_name_ = base::StringPrintf(L"WindowHanMonitorTest-%d",
                                      base::GetCurrentProcId());
    ASSERT_TRUE(window_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_UI, 0)));
  }

  void TearDown() override {
    DeleteMessageWindow();
    window_thread_.Stop();
  }

  void CreateMessageWindow() {
    bool succeeded = false;
    base::WaitableEvent created(true, false);
    ASSERT_TRUE(window_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&WindowHangMonitorTest::CreateMessageWindowInWorkerThread,
                   base::Unretained(this), window_name_, &succeeded,
                   &created)));
    created.Wait();
    ASSERT_TRUE(succeeded);
  }

  void DeleteMessageWindow() {
    base::WaitableEvent deleted(true, false);
    window_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&WindowHangMonitorTest::DeleteMessageWindowInWorkerThread,
                   base::Unretained(this), &deleted));
    deleted.Wait();
  }

  void WaitForPing() {
    while (true) {
      {
        base::AutoLock auto_lock(ping_lock_);
        if (pings_) {
          ping_event_.Reset();
          --pings_;
          return;
        }
      }
      ping_event_.Wait();
    }
  }

  HangMonitorThread& monitor_thread() { return monitor_thread_; }

  const base::win::MessageWindow* message_window() const {
    return message_window_.get();
  }

  const base::string16& window_name() const { return window_name_; }

  base::Thread* window_thread() { return &window_thread_; }

 private:
  bool MessageCallback(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       LRESULT* result) {
    EXPECT_EQ(window_thread_.message_loop(), base::MessageLoop::current());
    if (message == WM_NULL) {
      base::AutoLock auto_lock(ping_lock_);
      ++pings_;
      ping_event_.Signal();
    }

    return false;  // Pass through to DefWindowProc.
  }

  void CreateMessageWindowInWorkerThread(const base::string16& name,
                                         bool* success,
                                         base::WaitableEvent* created) {
    message_window_.reset(new base::win::MessageWindow);
    *success = message_window_->CreateNamed(
        base::Bind(&WindowHangMonitorTest::MessageCallback,
                   base::Unretained(this)),
        name);
    created->Signal();
  }

  void DeleteMessageWindowInWorkerThread(base::WaitableEvent* deleted) {
    message_window_.reset();
    if (deleted)
      deleted->Signal();
  }

  HangMonitorThread monitor_thread_;
  scoped_ptr<base::win::MessageWindow> message_window_;
  base::string16 window_name_;
  base::Lock ping_lock_;
  base::WaitableEvent ping_event_;
  size_t pings_;
  base::Thread window_thread_;

  DISALLOW_COPY_AND_ASSIGN(WindowHangMonitorTest);
};

}  // namespace

TEST_F(WindowHangMonitorTest, NoWindow) {
  base::CommandLine child_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  child_command_line.AppendSwitchASCII(switches::kTestChildProcess,
                                       "NoWindowChild");
  base::Process process =
      base::LaunchProcess(child_command_line, base::LaunchOptions());
  ASSERT_TRUE(process.IsValid());

  base::ScopedClosureRunner terminate_process_runner(
      base::Bind(base::IgnoreResult(&base::Process::Terminate),
                 base::Unretained(&process), 1, true));

  monitor_thread().Start(process.Duplicate(), window_name());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));

  terminate_process_runner.Reset();

  ASSERT_EQ(WindowHangMonitor::WINDOW_NOT_FOUND,
            monitor_thread().WaitForEvent());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));
}

TEST_F(WindowHangMonitorTest, WindowBeforeWatcher) {
  CreateMessageWindow();

  monitor_thread().Start(base::Process::Current(), window_name());

  WaitForPing();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));
}

TEST_F(WindowHangMonitorTest, WindowBeforeDestroy) {
  CreateMessageWindow();

  monitor_thread().Start(base::Process::Current(), window_name());

  WaitForPing();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));

  monitor_thread().DestroyWatcher();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(base::TimeDelta()));
}

TEST_F(WindowHangMonitorTest, NoWindowBeforeDestroy) {
  monitor_thread().Start(base::Process::Current(), window_name());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));
  monitor_thread().DestroyWatcher();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(base::TimeDelta()));
}

TEST_F(WindowHangMonitorTest, WatcherBeforeWindow) {
  monitor_thread().Start(base::Process::Current(), window_name());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));

  CreateMessageWindow();

  WaitForPing();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));
}

TEST_F(WindowHangMonitorTest, DetectsWindowDisappearance) {
  CreateMessageWindow();

  monitor_thread().Start(base::Process::Current(), window_name());

  WaitForPing();

  DeleteMessageWindow();

  ASSERT_EQ(WindowHangMonitor::WINDOW_VANISHED,
            monitor_thread().WaitForEvent());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));
}

TEST_F(WindowHangMonitorTest, DetectsWindowNameChange) {
  // This test changes the title of the message window as a proxy for what
  // happens if the window handle is reused for a different purpose. The latter
  // is impossible to test in a deterministic fashion.
  CreateMessageWindow();

  monitor_thread().Start(base::Process::Current(), window_name());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));

  ASSERT_TRUE(::SetWindowText(message_window()->hwnd(), L"Gonsky"));

  ASSERT_EQ(WindowHangMonitor::WINDOW_VANISHED,
            monitor_thread().WaitForEvent());
}

TEST_F(WindowHangMonitorTest, DetectsWindowHang) {
  CreateMessageWindow();

  monitor_thread().Start(base::Process::Current(), window_name());

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));

  // Block the worker thread.
  base::WaitableEvent hang(true, false);

  window_thread()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Wait, base::Unretained(&hang)));

  EXPECT_EQ(WindowHangMonitor::WINDOW_HUNG,
            monitor_thread().WaitForEvent());

  // Unblock the worker thread.
  hang.Signal();

  ASSERT_FALSE(monitor_thread().TimedWaitForEvent(
      base::TimeDelta::FromMilliseconds(150)));
}

}  // namespace browser_watcher
