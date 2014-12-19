// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_H_
#define CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_H_

#include <map>
#include "base/memory/ref_counted.h"
#include "content/browser/message_port_delegate.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

struct CrossOriginServiceWorkerClient;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

// Tracks all active navigator.connect connections, wakes up service workers
// when a message arrives, and routes the messages.
// One instance of this class exists per StoragePartition.
// TODO(mek): Clean up message ports when a service worker is unregistered.
// TODO(mek): Somehow clean up connections when the client side goes away.
class NavigatorConnectContext
    : public MessagePortDelegate,
      public base::RefCountedThreadSafe<NavigatorConnectContext> {
 public:
  explicit NavigatorConnectContext(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // Registers an accepted connection and updates MessagePortService with the
  // new route/delegate for the message port associated with this connection.
  void RegisterConnection(const CrossOriginServiceWorkerClient& client,
                          const scoped_refptr<ServiceWorkerRegistration>&
                              service_worker_registration);

  // MessagePortDelegate implementation. For the implementation of this class,
  // the route_id of a message port is always equal to the message_port_id.
  void SendMessage(int route_id,
                   const base::string16& message,
                   const std::vector<int>& sent_message_port_ids) override;
  void SendMessagesAreQueued(int route_id) override;

 private:
  friend class base::RefCountedThreadSafe<NavigatorConnectContext>;
  ~NavigatorConnectContext() override;

  // Callback called by SendMessage when a ServiceWorkerRegistration for the
  // given message port has been located.
  void DoSendMessage(const CrossOriginServiceWorkerClient& client,
                     const base::string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     ServiceWorkerStatusCode service_worker_status,
                     const scoped_refptr<ServiceWorkerRegistration>&
                         service_worker_registration);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  struct Connection;
  // Maps route ids to connections. For the purpose of this class, the route id
  // of a connection is the same as its message_port_id.
  std::map<int, Connection> connections_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_H_
