// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/image_capture_impl.h"

#include "base/bind_helpers.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
void ImageCaptureImpl::Create(
    mojo::InterfaceRequest<blink::mojom::ImageCapture> request) {
  // |binding_| will take ownership of ImageCaptureImpl.
  new ImageCaptureImpl(std::move(request));
}

ImageCaptureImpl::~ImageCaptureImpl() {}

void ImageCaptureImpl::TakePhoto(const mojo::String& /* source_id */,
                                 const TakePhotoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mcasas): Implement this feature., https://crbug.com/518807.
  mojo::Array<uint8_t> data(1);
  callback.Run("text/plain", std::move(data));
}

ImageCaptureImpl::ImageCaptureImpl(
    mojo::InterfaceRequest<blink::mojom::ImageCapture> request)
    : binding_(this, std::move(request)) {}

}  // namespace content
