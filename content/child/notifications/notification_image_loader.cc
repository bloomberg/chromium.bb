// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_image_loader.h"

#include "base/logging.h"
#include "content/child/child_thread.h"
#include "content/child/image_decoder.h"
#include "content/child/worker_task_runner.h"
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

NotificationImageLoader::~NotificationImageLoader() {}

void NotificationImageLoader::StartOnMainThread(const WebURL& image_url,
                                                int worker_thread_id) {
  DCHECK(ChildThread::current());
  DCHECK(!url_loader_);

  worker_thread_id_ = worker_thread_id;

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
  if (!worker_thread_id_) {
    callback_.Run(loader);
    return;
  }

  WorkerTaskRunner::Instance()->PostTask(
      worker_thread_id_,
      base::Bind(callback_, loader));
}

}  // namespace content
