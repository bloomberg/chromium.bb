// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_main_thread.h"

#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"

namespace content {

GpuMainThread::GpuMainThread(const std::string& channel_id)
    : base::Thread("Chrome_InProcGpuThread"),
      channel_id_(channel_id),
      gpu_process_(NULL) {
}

GpuMainThread::~GpuMainThread() {
  Stop();
}

void GpuMainThread::Init() {
  gpu_process_ = new GpuProcess();
  // The process object takes ownership of the thread object, so do not
  // save and delete the pointer.
  gpu_process_->set_main_thread(new GpuChildThread(channel_id_));
}

void GpuMainThread::CleanUp() {
  delete gpu_process_;
}

base::Thread* CreateGpuMainThread(const std::string& channel_id) {
  return new GpuMainThread(channel_id);
}

}  // namespace content
