// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/common/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorker.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace blink {
class WebServiceWorkerProxy;
}

namespace content {

class ThreadSafeSender;

// Each instance corresponds to one ServiceWorker object in JS context, and
// is held by ServiceWorker object in Blink's C++ layer via
// WebServiceWorker::Handle.
//
// Each instance holds one Mojo connection for interface
// blink::mojom::ServiceWorkerObjectHost inside |info_|, so the corresponding
// ServiceWorkerHandle doesn't go away in the browser process while the
// ServiceWorker object is alive.
class CONTENT_EXPORT WebServiceWorkerImpl
    : public blink::WebServiceWorker,
      public base::RefCounted<WebServiceWorkerImpl> {
 public:
  WebServiceWorkerImpl(blink::mojom::ServiceWorkerObjectInfoPtr info,
                       ThreadSafeSender* thread_safe_sender);

  void OnStateChanged(blink::mojom::ServiceWorkerState new_state);

  // blink::WebServiceWorker overrides.
  void SetProxy(blink::WebServiceWorkerProxy* proxy) override;
  blink::WebServiceWorkerProxy* Proxy() override;
  blink::WebURL Url() const override;
  blink::mojom::ServiceWorkerState GetState() const override;
  void PostMessageToWorker(
      blink::WebServiceWorkerProvider* provider,
      blink::TransferableMessage message,
      const blink::WebSecurityOrigin& source_origin) override;
  void Terminate() override;

  // Creates WebServiceWorker::Handle object that owns a reference to the given
  // WebServiceWorkerImpl object.
  static std::unique_ptr<blink::WebServiceWorker::Handle> CreateHandle(
      const scoped_refptr<WebServiceWorkerImpl>& worker);

 private:
  friend class base::RefCounted<WebServiceWorkerImpl>;
  ~WebServiceWorkerImpl() override;

  blink::mojom::ServiceWorkerObjectInfoPtr info_;
  blink::mojom::ServiceWorkerState state_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  blink::WebServiceWorkerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
