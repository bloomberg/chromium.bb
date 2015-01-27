// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_SERVICE_WORKER_SERVICE_FACTORY_H_
#define CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_SERVICE_WORKER_SERVICE_FACTORY_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/navigator_connect_service_factory.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

// Service worker specific NavigatorConnectServiceFactory implementation. This
// class accepts all URLs (and thus has to be the first factory to be added to
// NavigatorConnectContext or no other factories will ever by used).
// Tries to find a service worker registration to handle the url being connected
// to.
class NavigatorConnectServiceWorkerServiceFactory
    : public NavigatorConnectServiceFactory {
 public:
  explicit NavigatorConnectServiceWorkerServiceFactory(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);
  ~NavigatorConnectServiceWorkerServiceFactory() override;

  // NavigatorConnectServiceFactory implementation.
  bool HandlesUrl(const GURL& target_url) override;
  void Connect(const NavigatorConnectClient& client,
               const ConnectCallback& callback) override;

 private:
  // Callback called when the Service Worker context found (or didn't find) a
  // service worker registration to serve a particular URL.
  void GotServiceWorkerRegistration(
      const ConnectCallback& callback,
      const NavigatorConnectClient& client,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  // Callback called when the service worker finished handling the cross origin
  // connection event.
  void OnConnectResult(
      const ConnectCallback& callback,
      const NavigatorConnectClient& client,
      const scoped_refptr<ServiceWorkerRegistration>& registration,
      ServiceWorkerStatusCode status,
      bool accept_connection);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  base::WeakPtrFactory<NavigatorConnectServiceWorkerServiceFactory>
      weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_SERVICE_WORKER_SERVICE_FACTORY_H_
