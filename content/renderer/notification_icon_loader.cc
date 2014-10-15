// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notification_icon_loader.h"

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

NotificationIconLoader::NotificationIconLoader(
    int notification_id,
    const DownloadCompletedCallback& callback)
    : notification_id_(notification_id),
      callback_(callback),
      completed_(false) {}

NotificationIconLoader::~NotificationIconLoader() {}

void NotificationIconLoader::Start(const WebURL& icon_url) {
  DCHECK(!loader_);

  WebURLRequest request(icon_url);
  request.setRequestContext(WebURLRequest::RequestContextImage);

  loader_.reset(blink::Platform::current()->createURLLoader());
  loader_->loadAsynchronously(request, this);
}

void NotificationIconLoader::Cancel() {
  DCHECK(loader_);

  completed_ = true;
  loader_->cancel();
}

void NotificationIconLoader::didReceiveData(
    WebURLLoader* loader,
    const char* data,
    int data_length,
    int encoded_data_length) {
  DCHECK(!completed_);
  DCHECK_GT(data_length, 0);

  buffer_.insert(buffer_.end(), data, data + data_length);
}

void NotificationIconLoader::didFinishLoading(
    WebURLLoader* loader,
    double finish_time,
    int64_t total_encoded_data_length) {
  DCHECK(!completed_);

  SkBitmap icon;
  if (!buffer_.empty()) {
    ImageDecoder decoder;
    icon = decoder.Decode(&buffer_[0], buffer_.size());
  }

  completed_ = true;
  callback_.Run(notification_id_, icon);
}

void NotificationIconLoader::didFail(
    WebURLLoader* loader, const WebURLError& error) {
  if (completed_)
    return;

  completed_ = true;
  callback_.Run(notification_id_, SkBitmap());
}

}  // namespace content
