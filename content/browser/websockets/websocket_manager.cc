// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/websockets/websocket_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "services/shell/public/cpp/interface_registry.h"

namespace content {

namespace {

const char kWebSocketManagerKeyName[] = "web_socket_manager";

// Max number of pending connections per WebSocketManager used for per-renderer
// WebSocket throttling.
const int kMaxPendingWebSocketConnections = 255;

}  // namespace

class WebSocketManager::Handle : public base::SupportsUserData::Data {
 public:
  explicit Handle(WebSocketManager* manager) : manager_(manager) {}
  ~Handle() override {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, manager_);
  }
  WebSocketManager* manager() const { return manager_; }
 private:
  WebSocketManager* manager_;
};

// static
void WebSocketManager::CreateWebSocket(int process_id, int frame_id,
                                       mojom::WebSocketRequest request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderProcessHost* host = RenderProcessHost::FromID(process_id);
  CHECK(host);

  Handle* handle =
      static_cast<Handle*>(host->GetUserData(kWebSocketManagerKeyName));
  if (!handle) {
    handle = new Handle(
        new WebSocketManager(process_id, host->GetStoragePartition()));
    host->SetUserData(kWebSocketManagerKeyName, handle);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebSocketManager::DoCreateWebSocket,
                 base::Unretained(handle->manager()),
                 frame_id,
                 base::Passed(&request)));
}

WebSocketManager::WebSocketManager(int process_id,
                                   StoragePartition* storage_partition)
    : process_id_(process_id),
      storage_partition_(storage_partition),
      num_pending_connections_(0),
      num_current_succeeded_connections_(0),
      num_previous_succeeded_connections_(0),
      num_current_failed_connections_(0),
      num_previous_failed_connections_(0) {}

WebSocketManager::~WebSocketManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (auto impl : impls_) {
    impl->GoAway();
    delete impl;
  }
}

void WebSocketManager::DoCreateWebSocket(int frame_id,
                                         mojom::WebSocketRequest request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (num_pending_connections_ >= kMaxPendingWebSocketConnections) {
    // Too many websockets! By returning here, we let |request| die, which
    // will be observed by the client as Mojo connection error.
    return;
  }

  // Keep all WebSocketImpls alive until either the client drops its
  // connection (see OnLostConnectionToClient) or we need to shutdown.

  impls_.insert(CreateWebSocketImpl(this, std::move(request), frame_id,
                                    CalculateDelay()));
  ++num_pending_connections_;

  if (!throttling_period_timer_.IsRunning()) {
    throttling_period_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMinutes(2),
        this,
        &WebSocketManager::ThrottlingPeriodTimerCallback);
  }
}

// Calculate delay as described in the per-renderer WebSocket throttling
// design doc: https://goo.gl/tldFNn
base::TimeDelta WebSocketManager::CalculateDelay() const {
  int64_t f = num_previous_failed_connections_ +
              num_current_failed_connections_;
  int64_t s = num_previous_succeeded_connections_ +
              num_current_succeeded_connections_;
  int p = num_pending_connections_;
  return base::TimeDelta::FromMilliseconds(
      base::RandInt(1000, 5000) *
      (1 << std::min(p + f / (s + 1), INT64_C(16))) / 65536);
}

void WebSocketManager::ThrottlingPeriodTimerCallback() {
  num_previous_failed_connections_ = num_current_failed_connections_;
  num_current_failed_connections_ = 0;

  num_previous_succeeded_connections_ = num_current_succeeded_connections_;
  num_current_succeeded_connections_ = 0;

  if (num_pending_connections_ == 0 &&
      num_previous_failed_connections_ == 0 &&
      num_previous_succeeded_connections_ == 0) {
    throttling_period_timer_.Stop();
  }
}

WebSocketImpl* WebSocketManager::CreateWebSocketImpl(
    WebSocketImpl::Delegate* delegate,
    mojom::WebSocketRequest request,
    int frame_id,
    base::TimeDelta delay) {
  return new WebSocketImpl(delegate, std::move(request), frame_id, delay);
}

int WebSocketManager::GetClientProcessId() {
  return process_id_;
}

StoragePartition* WebSocketManager::GetStoragePartition() {
  return storage_partition_;
}

void WebSocketManager::OnReceivedResponseFromServer(WebSocketImpl* impl) {
  // The server accepted this WebSocket connection.
  impl->OnHandshakeSucceeded();
  --num_pending_connections_;
  DCHECK_GE(num_pending_connections_, 0);
  ++num_current_succeeded_connections_;
}

void WebSocketManager::OnLostConnectionToClient(WebSocketImpl* impl) {
  // The client is no longer interested in this WebSocket.
  if (!impl->handshake_succeeded()) {
    // Update throttling counters (failure).
    --num_pending_connections_;
    DCHECK_GE(num_pending_connections_, 0);
    ++num_current_failed_connections_;
  }
  impl->GoAway();
  impls_.erase(impl);
  delete impl;
}

}  // namespace content
