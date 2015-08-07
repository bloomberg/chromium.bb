// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_COMPONENT_DEVICE_CLIENT_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_COMPONENT_DEVICE_CLIENT_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chromecast/public/media/media_component_device.h"
#include "chromecast/public/media/video_pipeline_device.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromecast {
namespace media {

// Helper for implementing MediaComponentDevice::Client with
// a base::Callback.
class MediaComponentDeviceClientImpl : public MediaComponentDevice::Client {
 public:
  MediaComponentDeviceClientImpl(const base::Closure& eos_cb);
  ~MediaComponentDeviceClientImpl() override;

  void OnEndOfStream() override;

 private:
  base::Closure eos_cb_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_MEDIA_COMPONENT_DEVICE_CLIENT_IMPL_H_
