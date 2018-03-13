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
#include "third_party/WebKit/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorker.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebServiceWorkerProxy;
}

namespace content {

class ThreadSafeSender;

// Each instance corresponds to one ServiceWorker object in JS context, and
// is held by ServiceWorker object in Blink's C++ layer via
// WebServiceWorker::Handle.
//
// Each instance holds one Mojo connection (|host_for_global_scope_| or
// |host_for_client_|) for interface blink::mojom::ServiceWorkerObjectHost, so
// the corresponding ServiceWorkerHandle doesn't go away in the browser process
// while the ServiceWorker object is alive.
class CONTENT_EXPORT WebServiceWorkerImpl
    : public blink::WebServiceWorker,
      public base::RefCounted<WebServiceWorkerImpl> {
 public:
  // |io_task_runner| is used to bind |host_for_global_scope_| for service
  // worker execution context, as ServiceWorkerObjectHost is Channel-associated
  // interface and needs to be bound on either the main or IO thread.
  static scoped_refptr<WebServiceWorkerImpl> CreateForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerObjectInfoPtr info,
      ThreadSafeSender* thread_safe_sender,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  static scoped_refptr<WebServiceWorkerImpl> CreateForServiceWorkerClient(
      blink::mojom::ServiceWorkerObjectInfoPtr info,
      ThreadSafeSender* thread_safe_sender);

  void OnStateChanged(blink::mojom::ServiceWorkerState new_state);

  // blink::WebServiceWorker overrides.
  void SetProxy(blink::WebServiceWorkerProxy* proxy) override;
  blink::WebServiceWorkerProxy* Proxy() override;
  blink::WebURL Url() const override;
  blink::mojom::ServiceWorkerState GetState() const override;
  void PostMessageToServiceWorker(
      blink::TransferableMessage message,
      const blink::WebSecurityOrigin& source_origin) override;
  void TerminateForTesting(
      std::unique_ptr<TerminateForTestingCallback> callback) override;

  // Creates WebServiceWorker::Handle object that owns a reference to the given
  // WebServiceWorkerImpl object.
  static std::unique_ptr<blink::WebServiceWorker::Handle> CreateHandle(
      scoped_refptr<WebServiceWorkerImpl> worker);

 private:
  friend class base::RefCounted<WebServiceWorkerImpl>;
  WebServiceWorkerImpl(blink::mojom::ServiceWorkerObjectInfoPtr info,
                       ThreadSafeSender* thread_safe_sender);
  ~WebServiceWorkerImpl() override;

  blink::mojom::ServiceWorkerObjectHost* GetObjectHost();

  // Either |host_for_global_scope_| or |host_for_client_| is non-null.
  //
  // |host_for_global_scope_| is for service worker execution contexts. It is
  // used on the worker thread but bound on the IO thread, because it's a
  // channel-associated interface which can be bound only on the main or IO
  // thread.
  // TODO(leonhsl): Once we can detach this interface out from the legacy IPC
  // channel-associated interfaces world, we should bind it always on the worker
  // thread on which |this| lives.
  // Although it is a scoped_refptr, the only one owner is |this|.
  scoped_refptr<blink::mojom::ThreadSafeServiceWorkerObjectHostAssociatedPtr>
      host_for_global_scope_;
  // |host_for_client_| is for service worker clients (document).
  // It is bound and used on the main thread.
  blink::mojom::ServiceWorkerObjectHostAssociatedPtr host_for_client_;

  blink::mojom::ServiceWorkerObjectInfoPtr info_;
  blink::mojom::ServiceWorkerState state_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  blink::WebServiceWorkerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
