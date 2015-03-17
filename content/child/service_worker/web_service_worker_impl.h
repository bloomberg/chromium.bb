// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebServiceWorker.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace blink {
class WebServiceWorkerProxy;
}

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerMessageSender;
struct ServiceWorkerObjectInfo;

// Each instance corresponds to one ServiceWorker object in JS context, and
// is held by ServiceWorker object in blink's c++ layer, e.g. created one
// per navigator.serviceWorker.current or per successful
// navigator.serviceWorker.register call.
//
// Each instance holds one ServiceWorkerHandleReference so that
// corresponding ServiceWorkerHandle doesn't go away in the browser process
// while the ServiceWorker object is alive.
class WebServiceWorkerImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorker) {
 public:
  WebServiceWorkerImpl(scoped_ptr<ServiceWorkerHandleReference> handle_ref,
                       ServiceWorkerMessageSender* sender);
  virtual ~WebServiceWorkerImpl();

  void OnStateChanged(blink::WebServiceWorkerState new_state);

  virtual void setProxy(blink::WebServiceWorkerProxy* proxy);
  virtual blink::WebServiceWorkerProxy* proxy();
  virtual blink::WebURL url() const;
  virtual blink::WebServiceWorkerState state() const;
  virtual void postMessage(const blink::WebString& message,
                           blink::WebMessagePortChannelArray* channels);
  virtual void terminate();

 private:
  scoped_ptr<ServiceWorkerHandleReference> handle_ref_;
  blink::WebServiceWorkerState state_;
  scoped_refptr<ServiceWorkerMessageSender> sender_;
  blink::WebServiceWorkerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
