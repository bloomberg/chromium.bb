// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace content {

class BrowserThreadTest : public testing::Test {
 public:
  void Release() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    loop_.task_runner()->PostTask(FROM_HERE,
                                  base::MessageLoop::QuitWhenIdleClosure());
  }

  void StopUIThread() { ui_thread_->Stop(); }

 protected:
  void SetUp() override {
    ui_thread_ = std::make_unique<BrowserProcessSubThread>(BrowserThread::UI);
    ui_thread_->Start();

    io_thread_ = std::make_unique<BrowserProcessSubThread>(BrowserThread::IO);
    base::Thread::Options io_options;
    io_options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_->StartWithOptions(io_options);

    ui_thread_->RegisterAsBrowserThread();
    io_thread_->RegisterAsBrowserThread();
  }

  void TearDown() override {
    io_thread_.reset();
    ui_thread_.reset();

    BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::UI);
    BrowserThreadImpl::ResetGlobalsForTesting(BrowserThread::IO);
  }

  static void BasicFunction(base::MessageLoop* message_loop) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    message_loop->task_runner()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  class DeletedOnIO
      : public base::RefCountedThreadSafe<DeletedOnIO,
                                          BrowserThread::DeleteOnIOThread> {
   public:
    explicit DeletedOnIO(base::MessageLoop* message_loop)
        : message_loop_(message_loop) {}

   private:
    friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
    friend class base::DeleteHelper<DeletedOnIO>;

    ~DeletedOnIO() {
      CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
      message_loop_->task_runner()->PostTask(
          FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
    }

    base::MessageLoop* message_loop_;
  };

 private:
  std::unique_ptr<BrowserProcessSubThread> ui_thread_;
  std::unique_ptr<BrowserProcessSubThread> io_thread_;
  // It's kind of ugly to make this mutable - solely so we can post the Quit
  // Task from Release(). This should be fixed.
  mutable base::MessageLoop loop_;
};

class UIThreadDestructionObserver
    : public base::MessageLoop::DestructionObserver {
 public:
  explicit UIThreadDestructionObserver(bool* did_shutdown,
                                       const base::Closure& callback)
      : callback_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        callback_(callback),
        ui_task_runner_(
            BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)),
        did_shutdown_(did_shutdown) {
    BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)
        ->PostTask(FROM_HERE, base::BindOnce(&Watch, this));
  }

 private:
  static void Watch(UIThreadDestructionObserver* observer) {
    base::MessageLoop::current()->AddDestructionObserver(observer);
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    // Ensure that even during MessageLoop teardown the BrowserThread ID is
    // correctly associated with this thread and the BrowserThreadTaskRunner
    // knows it's on the right thread.
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_TRUE(ui_task_runner_->BelongsToCurrentThread());

    base::MessageLoop::current()->RemoveDestructionObserver(this);
    *did_shutdown_ = true;
    callback_task_runner_->PostTask(FROM_HERE, callback_);
  }

  const scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;
  const base::Closure callback_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  bool* did_shutdown_;
};

TEST_F(BrowserThreadTest, PostTask) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BasicFunction, base::MessageLoop::current()));
  base::RunLoop().Run();
}

TEST_F(BrowserThreadTest, Release) {
  BrowserThread::ReleaseSoon(BrowserThread::UI, FROM_HERE, this);
  base::RunLoop().Run();
}

TEST_F(BrowserThreadTest, ReleasedOnCorrectThread) {
  {
    scoped_refptr<DeletedOnIO> test(
        new DeletedOnIO(base::MessageLoop::current()));
  }
  base::RunLoop().Run();
}

TEST_F(BrowserThreadTest, PostTaskViaTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BasicFunction, base::MessageLoop::current()));
  base::RunLoop().Run();
}

TEST_F(BrowserThreadTest, ReleaseViaTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);
  task_runner->ReleaseSoon(FROM_HERE, this);
  base::RunLoop().Run();
}

TEST_F(BrowserThreadTest, PostTaskAndReply) {
  // Most of the heavy testing for PostTaskAndReply() is done inside the
  // task runner test.  This just makes sure we get piped through at all.
  ASSERT_TRUE(BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE, base::DoNothing(),
      base::BindOnce(&base::RunLoop::QuitCurrentWhenIdleDeprecated)));
  base::RunLoop().Run();
}

TEST_F(BrowserThreadTest, RunsTasksInCurrentSequencedDuringShutdown) {
  bool did_shutdown = false;
  base::RunLoop loop;
  UIThreadDestructionObserver observer(&did_shutdown, loop.QuitClosure());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserThreadTest::StopUIThread, base::Unretained(this)));
  loop.Run();

  EXPECT_TRUE(did_shutdown);
}

}  // namespace content
