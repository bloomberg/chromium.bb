// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebServiceWorker.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace content {
class ThreadSafeSender;

class WebServiceWorkerImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorker) {
 public:
  WebServiceWorkerImpl(int64 version_id,
                       ThreadSafeSender* thread_safe_sender)
      : version_id_(version_id),
        thread_safe_sender_(thread_safe_sender) {}
  virtual ~WebServiceWorkerImpl();

  virtual void postMessage(const blink::WebString& message,
                           blink::WebMessagePortChannelArray* channels);

 private:
  int64 version_id_;
  ThreadSafeSender* thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
