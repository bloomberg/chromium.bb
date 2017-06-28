// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_socket_service.h"

#include "base/memory/ptr_util.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/logger.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {
// Connect timeout for connect calls.
const int kConnectTimeoutSecs = 10;

// Ping interval
const int kPingIntervalInSecs = 5;

// Liveness timeout for connect calls, in milliseconds. If no message is
// received from the receiver during kConnectLivenessTimeoutSecs, it is
// considered gone.
const int kConnectLivenessTimeoutSecs = kPingIntervalInSecs * 2;
}  // namespace

namespace cast_channel {

int CastSocketService::last_channel_id_ = 0;

CastSocketService::CastSocketService()
    : RefcountedKeyedService(
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
      logger_(new Logger()) {
  DETACH_FROM_THREAD(thread_checker_);
}

CastSocketService::~CastSocketService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

scoped_refptr<Logger> CastSocketService::GetLogger() {
  return logger_;
}

CastSocket* CastSocketService::AddSocket(std::unique_ptr<CastSocket> socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(socket);
  int id = ++last_channel_id_;
  socket->set_id(id);

  auto* socket_ptr = socket.get();
  sockets_.insert(std::make_pair(id, std::move(socket)));
  return socket_ptr;
}

std::unique_ptr<CastSocket> CastSocketService::RemoveSocket(int channel_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(channel_id > 0);
  auto socket_it = sockets_.find(channel_id);

  std::unique_ptr<CastSocket> socket;
  if (socket_it != sockets_.end()) {
    socket = std::move(socket_it->second);
    sockets_.erase(socket_it);
  }
  return socket;
}

CastSocket* CastSocketService::GetSocket(int channel_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(channel_id > 0);
  const auto& socket_it = sockets_.find(channel_id);
  return socket_it == sockets_.end() ? nullptr : socket_it->second.get();
}

CastSocket* CastSocketService::GetSocket(
    const net::IPEndPoint& ip_endpoint) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = std::find_if(
      sockets_.begin(), sockets_.end(),
      [&ip_endpoint](
          const std::pair<const int, std::unique_ptr<CastSocket>>& pair) {
        return pair.second->ip_endpoint() == ip_endpoint;
      });
  return it == sockets_.end() ? nullptr : it->second.get();
}

int CastSocketService::OpenSocket(const net::IPEndPoint& ip_endpoint,
                                  net::NetLog* net_log,
                                  const base::TimeDelta& connect_timeout,
                                  const base::TimeDelta& liveness_timeout,
                                  const base::TimeDelta& ping_interval,
                                  uint64_t device_capabilities,
                                  const CastSocket::OnOpenCallback& open_cb,
                                  CastSocket::Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(observer);
  auto* socket = GetSocket(ip_endpoint);

  if (!socket) {
    // If cast socket does not exist.
    if (socket_for_test_) {
      socket = AddSocket(std::move(socket_for_test_));
    } else {
      socket = new CastSocketImpl(ip_endpoint, net_log, connect_timeout,
                                  liveness_timeout, ping_interval, logger_,
                                  device_capabilities);
      AddSocket(base::WrapUnique(socket));
    }
  }

  socket->AddObserver(observer);
  socket->Connect(open_cb);
  return socket->id();
}

int CastSocketService::OpenSocket(const net::IPEndPoint& ip_endpoint,
                                  net::NetLog* net_log,
                                  const CastSocket::OnOpenCallback& open_cb,
                                  CastSocket::Observer* observer) {
  auto connect_timeout = base::TimeDelta::FromSeconds(kConnectTimeoutSecs);
  auto ping_interval = base::TimeDelta::FromSeconds(kPingIntervalInSecs);
  auto liveness_timeout =
      base::TimeDelta::FromSeconds(kConnectLivenessTimeoutSecs);
  return OpenSocket(ip_endpoint, net_log, connect_timeout, liveness_timeout,
                    ping_interval, CastDeviceCapability::NONE, open_cb,
                    observer);
}

CastSocket::Observer* CastSocketService::GetObserver(const std::string& id) {
  auto it = socket_observer_map_.find(id);
  return it == socket_observer_map_.end() ? nullptr : it->second.get();
}

CastSocket::Observer* CastSocketService::AddObserver(
    const std::string& id,
    std::unique_ptr<CastSocket::Observer> observer) {
  CastSocket::Observer* observer_ptr = observer.get();
  socket_observer_map_.insert(std::make_pair(id, std::move(observer)));
  return observer_ptr;
}

void CastSocketService::SetSocketForTest(
    std::unique_ptr<cast_channel::CastSocket> socket_for_test) {
  socket_for_test_ = std::move(socket_for_test);
}

void CastSocketService::ShutdownOnUIThread() {}

}  // namespace cast_channel
