// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_MAIN_THREAD_H_
#define CONTENT_GPU_GPU_MAIN_THREAD_H_

#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {

class GpuProcess;

// This class creates a GPU thread (instead of a GPU process), when running
// with --in-process-gpu or --single-process.
class GpuMainThread : public base::Thread {
 public:
  explicit GpuMainThread(const std::string& channel_id);
  virtual ~GpuMainThread();

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  std::string channel_id_;
  // Deleted in CleanUp() on the gpu thread, so don't use smart pointers.
  GpuProcess* gpu_process_;

  DISALLOW_COPY_AND_ASSIGN(GpuMainThread);
};

CONTENT_EXPORT base::Thread* CreateGpuMainThread(const std::string& channel_id);

}  // namespace content

#endif  // CONTENT_GPU_GPU_MAIN_THREAD_H_
