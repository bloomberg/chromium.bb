// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/image_capture_impl.h"

#include "base/bind_helpers.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

namespace {

void RunMojoCallback(const ImageCaptureImpl::TakePhotoCallback& callback,
                     const std::string& mime_type,
                     mojo::Array<uint8_t> data) {
  callback.Run(mime_type, std::move(data));
}

void RunTakePhotoCallback(const ImageCaptureImpl::TakePhotoCallback& callback,
                          const std::string& mime_type,
                          std::unique_ptr<std::vector<uint8_t>> data) {
  DCHECK(data.get());
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RunMojoCallback, callback, mime_type,
                 base::Passed(mojo::Array<uint8_t>::From(*data))));
}

void TakePhotoOnIOThread(const mojo::String& source_id,
                         const ImageCaptureImpl::TakePhotoCallback& callback,
                         MediaStreamManager* media_stream_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == StreamDeviceInfo::kNoId ||
      !media_stream_manager->video_capture_manager()->TakePhoto(
          session_id, base::Bind(&RunTakePhotoCallback, callback))) {
    std::unique_ptr<std::vector<uint8_t>> empty_vector(
        new std::vector<uint8_t>());
    RunTakePhotoCallback(callback, "", std::move(empty_vector));
  }
}

}  // anonymous namespace

// static
void ImageCaptureImpl::Create(
    mojo::InterfaceRequest<blink::mojom::ImageCapture> request) {
  // |binding_| will take ownership of ImageCaptureImpl.
  new ImageCaptureImpl(std::move(request));
}

ImageCaptureImpl::~ImageCaptureImpl() {}

void ImageCaptureImpl::TakePhoto(const mojo::String& source_id,
                                 const TakePhotoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // media_stream_manager() can only be called on UI thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TakePhotoOnIOThread, source_id, callback,
                 BrowserMainLoop::GetInstance()->media_stream_manager()));
}

ImageCaptureImpl::ImageCaptureImpl(
    mojo::InterfaceRequest<blink::mojom::ImageCapture> request)
    : binding_(this, std::move(request)) {}

}  // namespace content
