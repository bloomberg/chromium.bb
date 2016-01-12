// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_ARC_VIDEO_SERVICE_HOST_H_
#define CONTENT_BROWSER_GPU_GPU_ARC_VIDEO_SERVICE_HOST_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/arc/video/video_host_delegate.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
struct ChannelHandle;
}

namespace content {

// This class passes requests from arc::VideoInstance to GpuArcVideoService to
// create a video accelerator channel in GPU process and reply the created
// channel.
//
// Don't be confused two senses of "host" of this class. This class, which
// implements arc::VideoHost, handles requests from arc::VideoInstance. At the
// same time, as its name says, this class is the host of corresponding class,
// GpuArcVideoService, in the GPU process.
class GpuArcVideoServiceHost : public arc::VideoHostDelegate {
 public:
  GpuArcVideoServiceHost();
  ~GpuArcVideoServiceHost() override;

  // arc::VideoHostDelegate implementation.
  void OnStopping() override;

  // arc::VideoHost implementation.
  void OnRequestArcVideoAcceleratorChannel(
      const OnRequestArcVideoAcceleratorChannelCallback& callback) override;

 private:
  base::ThreadChecker thread_checker_;

  // IO task runner, where GpuProcessHost tasks run.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoServiceHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_ARC_VIDEO_SERVICE_HOST_H_
