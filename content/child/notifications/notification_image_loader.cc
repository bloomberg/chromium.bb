// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_image_loader.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "content/child/image_decoder.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLRequest;

namespace content {

NotificationImageLoader::NotificationImageLoader(
    const ImageLoadCompletedCallback& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : callback_(callback),
      worker_task_runner_(worker_task_runner),
      main_task_runner_(main_task_runner),
      completed_(false) {}

NotificationImageLoader::~NotificationImageLoader() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void NotificationImageLoader::StartOnMainThread(const GURL& image_url) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!url_loader_);

  WebURL image_web_url(image_url);
  WebURLRequest request(image_web_url);
  request.setRequestContext(WebURLRequest::RequestContextImage);

  url_loader_.reset(blink::Platform::current()->createURLLoader());
  url_loader_->loadAsynchronously(request, this);
}

void NotificationImageLoader::didReceiveData(WebURLLoader* loader,
                                             const char* data,
                                             int data_length,
                                             int encoded_data_length) {
  DCHECK(!completed_);
  DCHECK_GT(data_length, 0);

  buffer_.insert(buffer_.end(), data, data + data_length);
}

void NotificationImageLoader::didFinishLoading(
    WebURLLoader* loader,
    double finish_time,
    int64_t total_encoded_data_length) {
  DCHECK(!completed_);

  RunCallbackOnWorkerThread();
}

void NotificationImageLoader::didFail(WebURLLoader* loader,
                                      const WebURLError& error) {
  if (completed_)
    return;

  RunCallbackOnWorkerThread();
}

void NotificationImageLoader::RunCallbackOnWorkerThread() {
  url_loader_.reset();

  completed_ = true;
  SkBitmap icon = GetDecodedImage();

  if (worker_task_runner_->BelongsToCurrentThread()) {
    callback_.Run(icon);
  } else {
    worker_task_runner_->PostTask(FROM_HERE, base::Bind(callback_, icon));
  }
}

SkBitmap NotificationImageLoader::GetDecodedImage() const {
  DCHECK(completed_);
  if (buffer_.empty())
    return SkBitmap();

  ImageDecoder decoder;
  return decoder.Decode(&buffer_[0], buffer_.size());
}

void NotificationImageLoader::DeleteOnCorrectThread() const {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }

  delete this;
}

}  // namespace content
