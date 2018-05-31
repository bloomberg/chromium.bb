// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "mojo/public/cpp/bindings/strong_associated_binding_set.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;

namespace service_worker_dispatcher_host_unittest {
class ServiceWorkerDispatcherHostTest;
class TestingServiceWorkerDispatcherHost;
FORWARD_DECLARE_TEST(ServiceWorkerDispatcherHostTest,
                     ProviderCreatedAndDestroyed);
FORWARD_DECLARE_TEST(ServiceWorkerDispatcherHostTest, CleanupOnRendererCrash);
}  // namespace service_worker_dispatcher_host_unittest

// ServiceWorkerDispatcherHost is a browser-side endpoint for the renderer to
// notify the browser ServiceWorkerProviderHost is created. In order to
// associate the Mojo interface with the legacy IPC channel,
// ServiceWorkerDispatcherHost overrides BrowserMessageFilter and
// BrowserAssociatedInterface.
//
// ServiceWorkerDispatcherHost is created on the UI thread in
// RenderProcessHostImpl::Init() via CreateMessageFilters(), but initialization,
// destruction, and Mojo calls occur on the IO thread. It lives as
// long as the renderer process lives.
//
// TODO(leonhsl): Remove this class once we can understand how to move
// OnProviderCreated() to an isolated message pipe.
class CONTENT_EXPORT ServiceWorkerDispatcherHost
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::ServiceWorkerDispatcherHost>,
      public mojom::ServiceWorkerDispatcherHost {
 public:
  ServiceWorkerDispatcherHost(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      int render_process_id);

  // BrowserMessageFilter implementation
  void OnFilterRemoved() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~ServiceWorkerDispatcherHost() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<ServiceWorkerDispatcherHost>;
  friend class service_worker_dispatcher_host_unittest::
      ServiceWorkerDispatcherHostTest;
  friend class service_worker_dispatcher_host_unittest::
      TestingServiceWorkerDispatcherHost;
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_dispatcher_host_unittest::ServiceWorkerDispatcherHostTest,
      ProviderCreatedAndDestroyed);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_dispatcher_host_unittest::ServiceWorkerDispatcherHostTest,
      CleanupOnRendererCrash);

  // mojom::ServiceWorkerDispatcherHost implementation
  void OnProviderCreated(ServiceWorkerProviderHostInfo info) override;

  ServiceWorkerContextCore* GetContext();

  const int render_process_id_;
  // Only accessed on the IO thread.
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
