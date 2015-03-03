// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/scoped_ipc_support.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"

namespace IPC {

namespace {

class IPCSupportInitializer : public mojo::embedder::ProcessDelegate {
 public:
  IPCSupportInitializer()
      : init_count_(0),
        shutting_down_(false) {
  }

  ~IPCSupportInitializer() override {}

  void Init(scoped_refptr<base::TaskRunner> io_thread_task_runner) {
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
      io_thread_task_runner_ = io_thread_task_runner;
      mojo::embedder::InitIPCSupport(mojo::embedder::ProcessType::NONE,
                                     io_thread_task_runner_,
                                     this, io_thread_task_runner_,
                                     mojo::embedder::ScopedPlatformHandle());
    }
  }

  void ShutDown() {
    base::AutoLock locker(lock_);
    DCHECK(init_count_ > 0);
    DCHECK(!shutting_down_);

    if (init_count_ > 1) {
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
          FROM_HERE,
          base::Bind(&IPCSupportInitializer::ShutDownOnIOThread,
                     base::Unretained(this)));
    }
  }

 private:
  void ShutDownOnIOThread() {
    base::AutoLock locker(lock_);
    if (shutting_down_) {
      DCHECK(init_count_ == 1);
      mojo::embedder::ShutdownIPCSupportOnIOThread();
      init_count_ = 0;
      shutting_down_ = false;
      io_thread_task_runner_ = nullptr;
   }
  }

  void OnShutdownComplete() override {}

  base::Lock lock_;
  size_t init_count_;
  bool shutting_down_;

  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(IPCSupportInitializer);
};

base::LazyInstance<IPCSupportInitializer>::Leaky ipc_support_initializer;

}  // namespace

ScopedIPCSupport::ScopedIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  ipc_support_initializer.Get().Init(io_thread_task_runner);
}

ScopedIPCSupport::~ScopedIPCSupport() {
  ipc_support_initializer.Get().ShutDown();
}

}  // namespace IPC
