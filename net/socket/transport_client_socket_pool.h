// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_tag.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

struct CommonConnectJobParams;

// TransportClientSocketPool establishes network connections through using
// ConnectJobs, and maintains a list of idle persistent sockets available for
// reuse. It restricts the number of sockets open at a time, both globally, and
// for each unique GroupId, which rougly corresponds to origin and privacy mode
// setting. TransportClientSocketPools is designed to work with HTTP reuse
// semantics, handling each request serially, before reusable sockets are
// returned to the socket pool.
//
// In order to manage connection limits on a per-Proxy basis, separate
// TransportClientSocketPools are created for each proxy, and another for
// connections that have no proxy.
class NET_EXPORT_PRIVATE TransportClientSocketPool
    : public ClientSocketPoolBaseHelper,
      public SSLConfigService::Observer {
 public:
  TransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      const CommonConnectJobParams* common_connect_job_params,
      SSLConfigService* ssl_config_service);

  ~TransportClientSocketPool() override;

  // Creates a socket pool with an alternative ConnectJobFactory, for use in
  // testing.
  //
  // |connect_backup_jobs_enabled| can be set to false to disable backup connect
  // jobs (Which are normally enabled).
  static std::unique_ptr<TransportClientSocketPool> CreateForTesting(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      std::unique_ptr<ConnectJobFactory> connect_job_factory,
      SSLConfigService* ssl_config_service,
      bool connect_backup_jobs_enabled);

  // ClientSocketPool implementation.
  int RequestSocket(const GroupId& group_id,
                    scoped_refptr<SocketParams> socket_params,
                    RequestPriority priority,
                    const SocketTag& socket_tag,
                    RespectLimits respect_limits,
                    ClientSocketHandle* handle,
                    CompletionOnceCallback callback,
                    const ProxyAuthCallback& proxy_auth_callback,
                    const NetLogWithSource& net_log) override;
  void RequestSockets(const GroupId& group_id,
                      scoped_refptr<SocketParams> socket_params,
                      int num_sockets,
                      const NetLogWithSource& net_log) override;

 protected:
  // Methods shared with WebSocketTransportClientSocketPool
  void NetLogTcpClientSocketPoolRequestedSocket(const NetLogWithSource& net_log,
                                                const GroupId& group_id);

 private:
  TransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      std::unique_ptr<ConnectJobFactory> connect_job_factory,
      SSLConfigService* ssl_config_service,
      bool connect_backup_jobs_enabled);

  class TransportConnectJobFactory : public ConnectJobFactory {
   public:
    TransportConnectJobFactory(
        const CommonConnectJobParams* common_connect_job_params);
    ~TransportConnectJobFactory() override;

    // ClientSocketPoolBase::ConnectJobFactory methods.

    std::unique_ptr<ConnectJob> NewConnectJob(
        RequestPriority request_priority,
        SocketTag socket_tag,
        scoped_refptr<SocketParams> socket_params,
        ConnectJob::Delegate* delegate) const override;

   private:
    const CommonConnectJobParams* common_connect_job_params_;

    DISALLOW_COPY_AND_ASSIGN(TransportConnectJobFactory);
  };

  // SSLConfigService::Observer methods.
  void OnSSLConfigChanged() override;

  SSLConfigService* const ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(TransportClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
