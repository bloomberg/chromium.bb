// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_MANAGER_H_
#define CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_MANAGER_H_

#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_getter_observer.h"
#include "services/network/websocket.h"
#include "services/network/websocket_throttler.h"

namespace content {
class StoragePartition;

// The WebSocketManager is a per child process instance that manages the
// lifecycle of network::WebSocket objects. It is responsible for creating
// network::WebSocket objects for each WebSocketRequest and throttling the
// number of network::WebSocket objects in use.
class CONTENT_EXPORT WebSocketManager
    : public net::URLRequestContextGetterObserver {
 public:
  // Called on the UI thread: create a websocket for a frame.
  static void CreateWebSocketForFrame(int process_id,
                                      int frame_id,
                                      network::mojom::WebSocketRequest request);

  // Called on the UI thread: create a websocket for a worker. Web workers of
  // any type (dedicated, shared, service worker) do not have a frame.
  static void CreateWebSocketWithOrigin(
      int process_id,
      url::Origin origin,
      network::mojom::WebSocketRequest request,
      int frame_id = MSG_ROUTING_NONE);

  // net::URLRequestContextGetterObserver implementation.
  void OnContextShuttingDown() override;

 protected:
  class Delegate;
  class Handle;
  friend class base::DeleteHelper<WebSocketManager>;

  // Called on the UI thread:
  WebSocketManager(int process_id, StoragePartition* storage_partition);

  // All other methods must run on the IO thread.

  ~WebSocketManager() override;
  void DoCreateWebSocket(int frame_id,
                         url::Origin origin,
                         network::mojom::WebSocketRequest request);
  void ThrottlingPeriodTimerCallback();

  // This is virtual to support testing.
  virtual std::unique_ptr<network::WebSocket> CreateWebSocket(
      std::unique_ptr<network::WebSocket::Delegate> delegate,
      network::mojom::WebSocketRequest request,
      network::WebSocketThrottler::PendingConnection pending_connection_tracker,
      int child_id,
      int frame_id,
      url::Origin origin,
      base::TimeDelta delay);

  net::URLRequestContext* GetURLRequestContext();
  virtual void OnLostConnectionToClient(network::WebSocket* impl);

  void ObserveURLRequestContextGetter();

  int process_id_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::set<std::unique_ptr<network::WebSocket>, base::UniquePtrComparator>
      impls_;

  // Timer and counters for per-renderer WebSocket throttling.
  base::RepeatingTimer throttling_period_timer_;

  network::WebSocketPerProcessThrottler throttler_;

  bool context_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_MANAGER_H_
