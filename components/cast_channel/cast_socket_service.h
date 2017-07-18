// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_SERVICE_H_
#define COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "components/cast_channel/cast_socket.h"
#include "content/public/browser/browser_thread.h"

namespace cast_channel {

// This class adds, removes, and returns cast sockets created by CastChannelAPI
// to underlying storage.
// Instance of this class is created on the UI thread and destroyed on the IO
// thread. All public API must be called from the IO thread.
class CastSocketService {
 public:
  static CastSocketService* GetInstance();

  // Returns a pointer to the Logger member variable.
  scoped_refptr<cast_channel::Logger> GetLogger();

  // Adds |socket| to |sockets_| and returns raw pointer of |socket|. Takes
  // ownership of |socket|.
  CastSocket* AddSocket(std::unique_ptr<CastSocket> socket);

  // Removes the CastSocket corresponding to |channel_id| from the
  // CastSocketRegistry. Returns nullptr if no such CastSocket exists.
  std::unique_ptr<CastSocket> RemoveSocket(int channel_id);

  // Returns the socket corresponding to |channel_id| if one exists, or nullptr
  // otherwise.
  virtual CastSocket* GetSocket(int channel_id) const;

  CastSocket* GetSocket(const net::IPEndPoint& ip_endpoint) const;

  // Opens cast socket with |ip_endpoint| and invokes |open_cb| when opening
  // operation finishes. If cast socket with |ip_endpoint| already exists,
  // invoke |open_cb| directly with existing socket's channel ID.
  // Parameters:
  // |ip_endpoint|: IP address and port of the remote host.
  // |net_log|: Log of socket events.
  // |connect_timeout|: Connection timeout interval.
  // |liveness_timeout|: Liveness timeout for connect calls.
  // |ping_interval|: Ping interval.
  // |logger|: Log of cast channel events.
  // |device_capabilities|: Device capabilities.
  // |open_cb|: OnOpenCallback invoked when cast socket is opened.
  // |observer|: Observer handles messages and errors on newly opened socket.
  // Does not take ownership of |observer|.
  int OpenSocket(const net::IPEndPoint& ip_endpoint,
                 net::NetLog* net_log,
                 const base::TimeDelta& connect_timeout,
                 const base::TimeDelta& liveness_timeout,
                 const base::TimeDelta& ping_interval,
                 uint64_t device_capabilities,
                 CastSocket::OnOpenCallback open_cb,
                 CastSocket::Observer* observer);

  // Opens cast socket with |ip_endpoint| and invokes |open_cb| when opening
  // operation finishes. If cast socket with |ip_endpoint| already exists,
  // invoke |open_cb| directly with existing socket's channel ID.
  // |ip_endpoint|: IP address and port of the remote host.
  // |net_log|: Log of socket events.
  // |open_cb|: OnOpenCallback invoked when cast socket is opened.
  // |observer|: Observer handles messages and errors on newly opened socket.
  // Does not take ownership of |observer|.
  virtual int OpenSocket(const net::IPEndPoint& ip_endpoint,
                         net::NetLog* net_log,
                         CastSocket::OnOpenCallback open_cb,
                         CastSocket::Observer* observer);

  // Remove |observer| from each socket in |sockets_|
  void RemoveObserver(CastSocket::Observer* observer);

  // Allow test to inject a mock cast socket.
  void SetSocketForTest(std::unique_ptr<CastSocket> socket_for_test);

 private:
  friend class CastSocketServiceTest;
  friend class MockCastSocketService;
  friend struct base::DefaultSingletonTraits<CastSocketService>;
  friend struct std::default_delete<CastSocketService>;

  CastSocketService();
  virtual ~CastSocketService();

  // Used to generate CastSocket id.
  static int last_channel_id_;

  // The collection of CastSocket keyed by channel_id.
  std::map<int, std::unique_ptr<CastSocket>> sockets_;

  scoped_refptr<Logger> logger_;

  std::unique_ptr<CastSocket> socket_for_test_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastSocketService);
};

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_CHANNEL_SERVICE_H_
