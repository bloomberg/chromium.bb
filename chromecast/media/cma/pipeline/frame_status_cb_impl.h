// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_FRAME_STATUS_CB_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_FRAME_STATUS_CB_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chromecast/public/media/media_component_device.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {

// Helper for implementing MediaComponentDevice::FrameStatusCB with
// a base::Callback.
class FrameStatusCBImpl : public MediaComponentDevice::FrameStatusCB {
 public:
  typedef base::Callback<void(MediaComponentDevice::FrameStatus)> CallbackType;

  FrameStatusCBImpl(const CallbackType& cb);
  ~FrameStatusCBImpl() override;

  void Run(MediaComponentDevice::FrameStatus status) override;

 private:
  CallbackType cb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_FRAME_STATUS_CB_IMPL_H_
