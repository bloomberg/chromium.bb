// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/ref_counted_video_capture_factory.h"

namespace content {

RefCountedVideoCaptureFactory::RefCountedVideoCaptureFactory(
    video_capture::mojom::DeviceFactoryPtr device_factory,
    base::OnceClosure destruction_cb)
    : device_factory_(std::move(device_factory)),
      destruction_cb_(std::move(destruction_cb)),
      weak_ptr_factory_(this) {}

RefCountedVideoCaptureFactory::~RefCountedVideoCaptureFactory() {
  std::move(destruction_cb_).Run();
}

base::WeakPtr<RefCountedVideoCaptureFactory>
RefCountedVideoCaptureFactory::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RefCountedVideoCaptureFactory::ReleaseFactoryForTesting() {
  device_factory_.reset();
}

}  // namespace content
