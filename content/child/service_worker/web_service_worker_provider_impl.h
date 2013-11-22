// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProviderClient.h"

namespace blink {
class WebURL;
}

namespace content {

class ThreadSafeSender;

class WebServiceWorkerProviderImpl
    : NON_EXPORTED_BASE(public blink::WebServiceWorkerProvider) {
 public:
  WebServiceWorkerProviderImpl(
      ThreadSafeSender* thread_safe_sender,
      scoped_ptr<blink::WebServiceWorkerProviderClient> client);
  virtual ~WebServiceWorkerProviderImpl();

  int provider_id() const { return provider_id_; }

  virtual void registerServiceWorker(const blink::WebURL& pattern,
                                     const blink::WebURL& script_url,
                                     WebServiceWorkerCallbacks*);

  virtual void unregisterServiceWorker(const blink::WebURL& pattern,
                                       WebServiceWorkerCallbacks*);

 private:
  const int provider_id_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_ptr<blink::WebServiceWorkerProviderClient> client_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerProviderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
