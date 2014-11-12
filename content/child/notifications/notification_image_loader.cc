// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_image_loader.h"

#include "base/logging.h"
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
    blink::WebNotificationDelegate* delegate,
    const ImageAvailableCallback& callback)
    : delegate_(delegate),
      callback_(callback),
      completed_(false) {}

NotificationImageLoader::~NotificationImageLoader() {
  completed_ = true;
}

void NotificationImageLoader::Start(const WebURL& image_url) {
  DCHECK(!loader_);

  WebURLRequest request(image_url);
  request.setRequestContext(WebURLRequest::RequestContextImage);

  loader_.reset(blink::Platform::current()->createURLLoader());
  loader_->loadAsynchronously(request, this);
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

  SkBitmap image;
  if (!buffer_.empty()) {
    ImageDecoder decoder;
    image = decoder.Decode(&buffer_[0], buffer_.size());
  }

  completed_ = true;
  callback_.Run(delegate_, image);
}

void NotificationImageLoader::didFail(WebURLLoader* loader,
                                      const WebURLError& error) {
  if (completed_)
    return;

  completed_ = true;
  callback_.Run(delegate_, SkBitmap());
}

}  // namespace content
