// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_REF_COUNTED_VIDEO_CAPTURE_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_REF_COUNTED_VIDEO_CAPTURE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"

namespace content {

// Enables ref-counted shared ownership of a
// video_capture::mojom::DeviceFactoryPtr.
// Since instances of this class do not guarantee that the connection stays open
// for its entire lifetime, clients must verify that the connection is bound
// before using it.
class CONTENT_EXPORT RefCountedVideoCaptureFactory
    : public base::RefCounted<RefCountedVideoCaptureFactory> {
 public:
  RefCountedVideoCaptureFactory(
      video_capture::mojom::DeviceFactoryPtr device_factory,
      base::OnceClosure destruction_cb);

  base::WeakPtr<RefCountedVideoCaptureFactory> GetWeakPtr();

  const video_capture::mojom::DeviceFactoryPtr& device_factory() {
    return device_factory_;
  }

  void ReleaseFactoryForTesting();

 private:
  friend class base::RefCounted<RefCountedVideoCaptureFactory>;
  ~RefCountedVideoCaptureFactory();

  video_capture::mojom::DeviceFactoryPtr device_factory_;
  base::OnceClosure destruction_cb_;
  base::WeakPtrFactory<RefCountedVideoCaptureFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedVideoCaptureFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_REF_COUNTED_VIDEO_CAPTURE_FACTORY_H_
