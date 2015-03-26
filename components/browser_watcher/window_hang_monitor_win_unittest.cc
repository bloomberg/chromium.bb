// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/window_hang_monitor_win.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/win/message_window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_watcher {

namespace {

class WindowHangMonitorTest : public testing::Test {
 public:
  typedef std::vector<WindowHangMonitor::WindowEvent> WindowEventVector;

  WindowHangMonitorTest()
      : monitor_(base::Bind(&WindowHangMonitorTest::OnWindowEvent,
                            base::Unretained(this))),
        message_loop_(base::MessageLoop::TYPE_UI),
        run_loop_(nullptr),
        pings_(0),
        worker_thread_("HangMan") {}

  // Callback from the hang detector.
  void OnWindowEvent(WindowHangMonitor::WindowEvent event) {
    // Record the event and terminate the message loop.
    events_.push_back(event);
    run_loop_->Quit();
  }

  void SetUp() override {
    // Pick a window name unique to this process.
    window_name_ = base::StringPrintf(L"WindowHanMonitorTest-%d",
                                      base::GetCurrentProcId());
    ASSERT_TRUE(worker_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_UI, 0)));

    // Set relatively short hang detection and ping intervals.
    monitor_.SetHangTimeoutForTesting(base::TimeDelta::FromMilliseconds(50));
    monitor_.SetPingIntervalForTesting(base::TimeDelta::FromMilliseconds(200));
  }

  void TearDown() override {
    DeleteMessageWindow();
    worker_thread_.Stop();
  }

  void CreateMessageWindow() {
    bool succeeded = false;
    base::WaitableEvent created(true, false);
    ASSERT_TRUE(worker_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&WindowHangMonitorTest::CreateMessageWindowInWorkerThread,
                   base::Unretained(this), window_name_, &succeeded,
                   &created)));
    created.Wait();
    ASSERT_TRUE(succeeded);
  }

  void DeleteMessageWindow() {
    base::WaitableEvent deleted(true, false);
    worker_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&WindowHangMonitorTest::DeleteMessageWindowInWorkerThread,
                   base::Unretained(this), &deleted));
    deleted.Wait();
  }

  bool MessageCallback(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       LRESULT* result) {
    EXPECT_EQ(worker_thread_.message_loop(), base::MessageLoop::current());
    if (message == WM_NULL)
      ++pings_;

    return false;  // Pass through to DefWindowProc.
  }

  void RunMessageLoop() {
    ASSERT_FALSE(run_loop_);

    base::RunLoop loop;

    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

  WindowHangMonitor* monitor() { return &monitor_; }
  const WindowEventVector& events() const { return events_; }
  const base::win::MessageWindow* message_window() const {
    return message_window_.get();
  }
  size_t pings() const { return pings_; }
  const base::string16& window_name() const { return window_name_; }
  base::Thread* worker_thread() { return &worker_thread_; }

 private:
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

  WindowHangMonitor monitor_;
  WindowEventVector events_;

  // Message and run loops for the main thread.
  base::MessageLoop message_loop_;
  base::RunLoop* run_loop_;
  scoped_ptr<base::win::MessageWindow> message_window_;
  base::string16 window_name_;
  size_t pings_;
  base::Thread worker_thread_;
};

}  // namespace

TEST_F(WindowHangMonitorTest, InitFailsWhenNoWindow) {
  ASSERT_FALSE(monitor()->Initialize(window_name()));
  EXPECT_TRUE(monitor()->IsIdleForTesting());
  EXPECT_EQ(0, pings());
  EXPECT_EQ(0, events().size());
}

TEST_F(WindowHangMonitorTest, InitSucceedsWhenWindow) {
  CreateMessageWindow();

  ASSERT_TRUE(monitor()->Initialize(window_name()));
  EXPECT_FALSE(monitor()->IsIdleForTesting());

  // Delete the window to synchronize against any pending message pings.
  DeleteMessageWindow();

  EXPECT_EQ(1, pings());
  EXPECT_EQ(0, events().size());
}

TEST_F(WindowHangMonitorTest, DetectsWindowDisappearance) {
  CreateMessageWindow();

  EXPECT_TRUE(monitor()->Initialize(window_name()));
  EXPECT_EQ(0, events().size());

  DeleteMessageWindow();

  RunMessageLoop();

  EXPECT_TRUE(monitor()->IsIdleForTesting());
  ASSERT_EQ(1, events().size());
  EXPECT_EQ(WindowHangMonitor::WINDOW_VANISHED, events()[0]);
}

TEST_F(WindowHangMonitorTest, DetectsWindowNameChange) {
  // This test changes the title of the message window as a proxy for what
  // happens if the window handle is reused for a different purpose. The latter
  // is impossible to test in a deterministic fashion.
  CreateMessageWindow();

  ASSERT_TRUE(monitor()->Initialize(window_name()));
  EXPECT_EQ(0, events().size());

  ASSERT_TRUE(::SetWindowText(message_window()->hwnd(), L"Gonsky"));

  RunMessageLoop();

  EXPECT_TRUE(monitor()->IsIdleForTesting());
  ASSERT_EQ(1, events().size());
  EXPECT_EQ(WindowHangMonitor::WINDOW_VANISHED, events()[0]);
}

TEST_F(WindowHangMonitorTest, DetectsWindowHang) {
  CreateMessageWindow();

  ASSERT_TRUE(monitor()->Initialize(window_name()));
  EXPECT_EQ(0, events().size());

  // Block the worker thread.
  base::WaitableEvent hang(true, false);

  worker_thread()->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Wait, base::Unretained(&hang)));

  RunMessageLoop();

  // Unblock the worker thread.
  hang.Signal();

  EXPECT_TRUE(monitor()->IsIdleForTesting());
  ASSERT_EQ(1, events().size());
  EXPECT_EQ(WindowHangMonitor::WINDOW_HUNG, events()[0]);
}

}  // namespace browser_watcher
