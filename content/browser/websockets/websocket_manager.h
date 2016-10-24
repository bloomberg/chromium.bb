// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_MANAGER_H_
#define CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_MANAGER_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/browser/websockets/websocket_impl.h"
#include "content/common/content_export.h"

namespace content {
class StoragePartition;

// The WebSocketManager is a per child process instance that manages the
// lifecycle of WebSocketImpl objects. It is responsible for creating
// WebSocketImpl objects for each WebSocketRequest and throttling the number of
// WebSocketImpl objects in use.
class CONTENT_EXPORT WebSocketManager
    : NON_EXPORTED_BASE(public WebSocketImpl::Delegate) {
 public:
  // Called on the UI thread:
  static void CreateWebSocket(int process_id, int frame_id,
                              blink::mojom::WebSocketRequest request);

 protected:
  class Handle;
  friend class base::DeleteHelper<WebSocketManager>;

  // Called on the UI thread:
  WebSocketManager(int process_id, StoragePartition* storage_partition);

  // All other methods must run on the IO thread.

  ~WebSocketManager() override;
  void DoCreateWebSocket(int frame_id, blink::mojom::WebSocketRequest request);
  base::TimeDelta CalculateDelay() const;
  void ThrottlingPeriodTimerCallback();

  // This is virtual to support testing.
  virtual WebSocketImpl* CreateWebSocketImpl(
      WebSocketImpl::Delegate* delegate,
      blink::mojom::WebSocketRequest request,
      int child_id,
      int frame_id,
      base::TimeDelta delay);

  // WebSocketImpl::Delegate methods:
  int GetClientProcessId() override;
  StoragePartition* GetStoragePartition() override;
  void OnReceivedResponseFromServer(WebSocketImpl* impl) override;
  void OnLostConnectionToClient(WebSocketImpl* impl) override;

  int process_id_;
  StoragePartition* storage_partition_;

  std::set<WebSocketImpl*> impls_;

  // Timer and counters for per-renderer WebSocket throttling.
  base::RepeatingTimer throttling_period_timer_;

  // The current number of pending connections.
  int num_pending_connections_;

  // The number of handshakes that failed in the current and previous time
  // period.
  int64_t num_current_succeeded_connections_;
  int64_t num_previous_succeeded_connections_;

  // The number of handshakes that succeeded in the current and previous time
  // period.
  int64_t num_current_failed_connections_;
  int64_t num_previous_failed_connections_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_MANAGER_H_
