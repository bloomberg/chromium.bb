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
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

namespace {

template<typename R, typename... Args>
void RunMojoCallback(const mojo::Callback<R(Args...)>& callback, Args... args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  callback.Run(std::forward<Args>(args)...);
}

void RunFailedGetCapabilitiesCallback(
    const ImageCaptureImpl::GetCapabilitiesCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::mojom::PhotoCapabilitiesPtr empty_capabilities =
      blink::mojom::PhotoCapabilities::New();
  empty_capabilities->zoom = blink::mojom::Range::New();
  cb.Run(std::move(empty_capabilities));
}

void RunTakePhotoCallbackOnUIThread(
    const ImageCaptureImpl::TakePhotoCallback& callback,
    mojo::String mime_type,
    mojo::Array<uint8_t> data) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RunMojoCallback<void, mojo::String, mojo::Array<uint8_t>>,
                 callback, mime_type, base::Passed(std::move(data))));
}

void RunFailedTakePhotoCallback(const ImageCaptureImpl::TakePhotoCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cb.Run("", mojo::Array<uint8_t>());
}

void TakePhotoOnIOThread(
    const mojo::String& source_id,
    MediaStreamManager* media_stream_manager,
    media::ScopedResultCallback<ImageCaptureImpl::TakePhotoCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == StreamDeviceInfo::kNoId)
    return;
  media_stream_manager->video_capture_manager()->TakePhoto(session_id,
                                                           std::move(callback));
}

}  // anonymous namespace

// static
void ImageCaptureImpl::Create(
    mojo::InterfaceRequest<blink::mojom::ImageCapture> request) {
  // |binding_| will take ownership of ImageCaptureImpl.
  new ImageCaptureImpl(std::move(request));
}

ImageCaptureImpl::~ImageCaptureImpl() {}

void ImageCaptureImpl::GetCapabilities(
    const mojo::String& source_id,
    const GetCapabilitiesCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<GetCapabilitiesCallback> scoped_callback(
      callback,
      media::BindToCurrentLoop(base::Bind(&RunFailedGetCapabilitiesCallback)));
}

void ImageCaptureImpl::TakePhoto(const mojo::String& source_id,
                                 const TakePhotoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<TakePhotoCallback> scoped_callback(
      base::Bind(&RunTakePhotoCallbackOnUIThread, callback),
      media::BindToCurrentLoop(base::Bind(&RunFailedTakePhotoCallback)));

  // media_stream_manager() can only be called on UI thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TakePhotoOnIOThread, source_id,
                 BrowserMainLoop::GetInstance()->media_stream_manager(),
                 base::Passed(&scoped_callback)));
}

ImageCaptureImpl::ImageCaptureImpl(
    mojo::InterfaceRequest<blink::mojom::ImageCapture> request)
    : binding_(this, std::move(request)) {}

}  // namespace content
