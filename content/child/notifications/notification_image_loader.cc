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
#include "third_party/skia/include/core/SkBitmap.h"

using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebURLRequest;

namespace content {

NotificationImageLoader::NotificationImageLoader(
    const NotificationImageLoadedCallback& callback)
    : callback_(callback),
      completed_(false) {}

NotificationImageLoader::~NotificationImageLoader() {
  // The WebURLLoader instance must be destroyed on the same thread it was
  // created on, being the main thread.
  if (!main_thread_task_runner_->RunsTasksOnCurrentThread())
    main_thread_task_runner_->DeleteSoon(FROM_HERE, url_loader_.release());
}

void NotificationImageLoader::StartOnMainThread(
    const WebURL& image_url,
    const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(!url_loader_);
  DCHECK(worker_task_runner);

  worker_task_runner_ = worker_task_runner;
  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  WebURLRequest request(image_url);
  request.setRequestContext(WebURLRequest::RequestContextImage);

  url_loader_.reset(blink::Platform::current()->createURLLoader());
  url_loader_->loadAsynchronously(request, this);
}

SkBitmap NotificationImageLoader::GetDecodedImage() const {
  if (buffer_.empty())
    return SkBitmap();

  ImageDecoder decoder;
  return decoder.Decode(&buffer_[0], buffer_.size());
}

void NotificationImageLoader::didReceiveData(
    WebURLLoader* loader,
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
  scoped_refptr<NotificationImageLoader> loader = make_scoped_refptr(this);
  if (worker_task_runner_->BelongsToCurrentThread())
    callback_.Run(loader);
  else
    worker_task_runner_->PostTask(FROM_HERE, base::Bind(callback_, loader));
}

}  // namespace content
