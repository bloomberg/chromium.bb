// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/scoped_ipc_support.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"

namespace IPC {

namespace {
// TODO(use_chrome_edk)
//class IPCSupportInitializer : public mojo::edk::ProcessDelegate {
class IPCSupportInitializer : public mojo::embedder::ProcessDelegate {
 public:
  IPCSupportInitializer()
      : init_count_(0),
        shutting_down_(false),
        was_shut_down_(false),
        observer_(nullptr) {}

  ~IPCSupportInitializer() override { DCHECK(!observer_); }

  void Init(scoped_refptr<base::TaskRunner> io_thread_task_runner);
  void ShutDown(bool force);

 private:
  // This watches for destruction of the MessageLoop that IPCSupportInitializer
  // uses for IO, and guarantees that the initializer is shut down if it still
  // exists when the loop is being destroyed.
  class MessageLoopObserver : public base::MessageLoop::DestructionObserver {
   public:
    MessageLoopObserver(IPCSupportInitializer* initializer)
        : initializer_(initializer) {}

    ~MessageLoopObserver() override {
      base::MessageLoop::current()->RemoveDestructionObserver(this);
    }

   private:
    // base::MessageLoop::DestructionObserver:
    void WillDestroyCurrentMessageLoop() override {
      initializer_->ShutDown(true);
    }

    IPCSupportInitializer* initializer_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopObserver);
  };

  void ShutDownOnIOThread();

  // mojo::embedder::ProcessDelegate:
  void OnShutdownComplete() override {}

  static void WatchMessageLoopOnIOThread(MessageLoopObserver* observer);

  base::Lock lock_;
  size_t init_count_;
  bool shutting_down_;

  // This is used to track whether shutdown has occurred yet, since we can be
  // shut down by either the scoper or IO MessageLoop destruction.
  bool was_shut_down_;

  // The message loop destruction observer we have watching our IO loop. This
  // is created on the initializer's own thread but is used and destroyed on the
  // IO thread.
  MessageLoopObserver* observer_;

  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(IPCSupportInitializer);
};

void IPCSupportInitializer::Init(
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  base::AutoLock locker(lock_);
  DCHECK((init_count_ == 0 && !io_thread_task_runner_) ||
         io_thread_task_runner_ == io_thread_task_runner);

  if (shutting_down_) {
    // If reinitialized before a pending shutdown task is executed, we
    // effectively cancel the shutdown task.
    DCHECK(init_count_ == 1);
    shutting_down_ = false;
    return;
  }

  init_count_++;
  if (init_count_ == 1) {
    was_shut_down_ = false;
    observer_ = new MessageLoopObserver(this);
    io_thread_task_runner_ = io_thread_task_runner;
    io_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WatchMessageLoopOnIOThread, observer_));
    mojo::embedder::InitIPCSupport(
        mojo::embedder::ProcessType::NONE, this, io_thread_task_runner_,
        mojo::embedder::ScopedPlatformHandle());
  }
}

void IPCSupportInitializer::ShutDown(bool force) {
  base::AutoLock locker(lock_);
  if (shutting_down_ || was_shut_down_)
    return;
  DCHECK(init_count_ > 0);
  if (init_count_ > 1 && !force) {
    init_count_--;
    return;
  }

  shutting_down_ = true;
  if (base::MessageLoop::current() &&
      base::MessageLoop::current()->task_runner() == io_thread_task_runner_) {
    base::AutoUnlock unlocker_(lock_);
    ShutDownOnIOThread();
  } else {
    io_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IPCSupportInitializer::ShutDownOnIOThread,
                              base::Unretained(this)));
  }
}

void IPCSupportInitializer::ShutDownOnIOThread() {
  base::AutoLock locker(lock_);
  if (shutting_down_ && !was_shut_down_) {
    mojo::embedder::ShutdownIPCSupportOnIOThread();
    init_count_ = 0;
    shutting_down_ = false;
    io_thread_task_runner_ = nullptr;
    was_shut_down_ = true;
    if (observer_) {
      delete observer_;
      observer_ = nullptr;
    }
  }
}

// static
void IPCSupportInitializer::WatchMessageLoopOnIOThread(
    MessageLoopObserver* observer) {
  base::MessageLoop::current()->AddDestructionObserver(observer);
}

base::LazyInstance<IPCSupportInitializer>::Leaky ipc_support_initializer;

}  // namespace

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  ipc_support_initializer.Get().Init(io_thread_task_runner);
}

ScopedIPCSupport::~ScopedIPCSupport() {
  ipc_support_initializer.Get().ShutDown(false);
}

}  // namespace IPC
