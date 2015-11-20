// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_image_loader.h"

#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/child_thread_impl.h"
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
    const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner)
    : callback_(callback),
      worker_task_runner_(worker_task_runner),
      notification_id_(0),
      completed_(false) {}

NotificationImageLoader::~NotificationImageLoader() {
  if (main_thread_task_runner_)
    DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
}

void NotificationImageLoader::StartOnMainThread(int notification_id,
                                                const GURL& image_url) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!url_loader_);

  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  notification_id_ = notification_id;

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
    callback_.Run(notification_id_, icon);
  } else {
    worker_task_runner_->PostTask(
        FROM_HERE, base::Bind(callback_, notification_id_, icon));
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
  if (!ChildThreadImpl::current()) {
    main_thread_task_runner_->DeleteSoon(FROM_HERE, this);
    return;
  }

  delete this;
}

}  // namespace content
