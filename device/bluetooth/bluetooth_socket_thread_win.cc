// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_thread_win.h"

#include "base/lazy_instance.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"

namespace device {

base::LazyInstance<scoped_refptr<BluetoothSocketThreadWin> > g_instance =
    LAZY_INSTANCE_INITIALIZER;

// static
scoped_refptr<BluetoothSocketThreadWin> BluetoothSocketThreadWin::Get() {
  if (!g_instance.Get().get()) {
    g_instance.Get() = new BluetoothSocketThreadWin();
  }
  return g_instance.Get();
}

BluetoothSocketThreadWin::BluetoothSocketThreadWin()
    : active_socket_count_(0) {}

BluetoothSocketThreadWin::~BluetoothSocketThreadWin() {}

void BluetoothSocketThreadWin::OnSocketActivate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_socket_count_++;
  EnsureStarted();
}

void BluetoothSocketThreadWin::OnSocketDeactivate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_socket_count_--;
  if (active_socket_count_ == 0 && thread_) {
    thread_->Stop();
    thread_.reset(NULL);
    task_runner_ = NULL;
  }
}

void BluetoothSocketThreadWin::EnsureStarted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (thread_)
    return;

  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_.reset(new base::Thread("BluetoothSocketThreadWin"));
  thread_->StartWithOptions(thread_options);
  task_runner_ = thread_->message_loop_proxy();
}

scoped_refptr<base::SequencedTaskRunner> BluetoothSocketThreadWin::task_runner()
    const {
  DCHECK(active_socket_count_ > 0);
  DCHECK(thread_);
  DCHECK(task_runner_);

  return task_runner_;
}

}  // namespace device
