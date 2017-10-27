// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/image_capture/image_capture_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/scoped_callback_runner.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

media::mojom::PhotoStatePtr MakeEmptyCapabilities() {
  media::mojom::PhotoStatePtr empty_capabilities =
      media::mojom::PhotoState::New();
  empty_capabilities->iso = media::mojom::Range::New();
  empty_capabilities->width = media::mojom::Range::New();
  empty_capabilities->height = media::mojom::Range::New();
  empty_capabilities->zoom = media::mojom::Range::New();
  empty_capabilities->exposure_compensation = media::mojom::Range::New();
  empty_capabilities->color_temperature = media::mojom::Range::New();
  empty_capabilities->brightness = media::mojom::Range::New();
  empty_capabilities->contrast = media::mojom::Range::New();
  empty_capabilities->saturation = media::mojom::Range::New();
  empty_capabilities->sharpness = media::mojom::Range::New();
  return empty_capabilities;
}

void GetPhotoStateOnIOThread(const std::string& source_id,
                             MediaStreamManager* media_stream_manager,
                             ImageCaptureImpl::GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

// TODO(mcasas): Enable PhotoState collection in Windows when understood why it
// prevents normal capture https://crbug.com/722038.
#if !defined(OS_WIN)
  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == MediaStreamDevice::kNoId)
    return;
  media_stream_manager->video_capture_manager()->GetPhotoState(
      session_id, std::move(callback));
#endif
}

void SetOptionsOnIOThread(const std::string& source_id,
                          MediaStreamManager* media_stream_manager,
                          media::mojom::PhotoSettingsPtr settings,
                          ImageCaptureImpl::SetOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == MediaStreamDevice::kNoId)
    return;
  media_stream_manager->video_capture_manager()->SetPhotoOptions(
      session_id, std::move(settings), std::move(callback));
}

void TakePhotoOnIOThread(const std::string& source_id,
                         MediaStreamManager* media_stream_manager,
                         ImageCaptureImpl::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == MediaStreamDevice::kNoId)
    return;
  media_stream_manager->video_capture_manager()->TakePhoto(session_id,
                                                           std::move(callback));
}

}  // anonymous namespace

ImageCaptureImpl::ImageCaptureImpl() {}

ImageCaptureImpl::~ImageCaptureImpl() {}

// static
void ImageCaptureImpl::Create(
    media::mojom::ImageCaptureRequest request) {
  if (!base::FeatureList::IsEnabled(features::kImageCaptureAPI))
    return;

  mojo::MakeStrongBinding(std::make_unique<ImageCaptureImpl>(),
                          std::move(request));
}

void ImageCaptureImpl::GetPhotoState(const std::string& source_id,
                                     GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetPhotoStateCallback scoped_callback = media::ScopedCallbackRunner(
      media::BindToCurrentLoop(std::move(callback)), MakeEmptyCapabilities());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&GetPhotoStateOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

void ImageCaptureImpl::SetOptions(const std::string& source_id,
                                  media::mojom::PhotoSettingsPtr settings,
                                  SetOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SetOptionsCallback scoped_callback = media::ScopedCallbackRunner(
      media::BindToCurrentLoop(std::move(callback)), false);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetOptionsOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(settings), std::move(scoped_callback)));
}

void ImageCaptureImpl::TakePhoto(const std::string& source_id,
                                 TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TakePhotoCallback scoped_callback = media::ScopedCallbackRunner(
      media::BindToCurrentLoop(std::move(callback)), media::mojom::Blob::New());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&TakePhotoOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

}  // namespace content
