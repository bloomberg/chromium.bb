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
class HttpProxySocketParams;
class SOCKSSocketParams;
class SSLSocketParams;
class TransportSocketParams;

class NET_EXPORT_PRIVATE TransportClientSocketPool
    : public ClientSocketPool,
      public SSLConfigService::Observer {
 public:
  // Callback to create a ConnectJob using the provided arguments. The lower
  // level parameters used to construct the ConnectJob (like hostname, type of
  // socket, proxy, etc) are all already bound to the callback.  If
  // |websocket_endpoint_lock_manager| is non-null, a ConnectJob for use by
  // WebSockets should be created.
  using CreateConnectJobCallback =
      base::RepeatingCallback<std::unique_ptr<ConnectJob>(
          RequestPriority priority,
          const SocketTag& socket_tag,
          const CommonConnectJobParams* common_connect_job_params,
          ConnectJob::Delegate* delegate)>;

  // "Parameters" that own a single callback for creating a ConnectJob that can
  // be of any type.
  //
  // TODO(mmenke): Once all the socket pool subclasses have been merged, replace
  // this class with a callback.
  class NET_EXPORT_PRIVATE SocketParams
      : public base::RefCounted<SocketParams> {
   public:
    explicit SocketParams(
        const CreateConnectJobCallback& create_connect_job_callback);

    const CreateConnectJobCallback& create_connect_job_callback() {
      return create_connect_job_callback_;
    }

    static scoped_refptr<SocketParams> CreateFromTransportSocketParams(
        scoped_refptr<TransportSocketParams> transport_client_params);

    static scoped_refptr<SocketParams> CreateFromSOCKSSocketParams(
        scoped_refptr<SOCKSSocketParams> socks_socket_params);

    static scoped_refptr<SocketParams> CreateFromSSLSocketParams(
        scoped_refptr<SSLSocketParams> ssl_socket_params);

    static scoped_refptr<SocketParams> CreateFromHttpProxySocketParams(
        scoped_refptr<HttpProxySocketParams> http_proxy_socket_params);

   private:
    friend class base::RefCounted<SocketParams>;
    ~SocketParams();

    const CreateConnectJobCallback create_connect_job_callback_;

    DISALLOW_COPY_AND_ASSIGN(SocketParams);
  };

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
      std::unique_ptr<ClientSocketPoolBase<SocketParams>::ConnectJobFactory>
          connect_job_factory,
      SSLConfigService* ssl_config_service,
      bool connect_backup_jobs_enabled);

  // ClientSocketPool implementation.
  int RequestSocket(const GroupId& group_id,
                    const void* resolve_info,
                    RequestPriority priority,
                    const SocketTag& socket_tag,
                    RespectLimits respect_limits,
                    ClientSocketHandle* handle,
                    CompletionOnceCallback callback,
                    const ProxyAuthCallback& proxy_auth_callback,
                    const NetLogWithSource& net_log) override;
  void RequestSockets(const GroupId& group_id,
                      const void* params,
                      int num_sockets,
                      const NetLogWithSource& net_log) override;
  void SetPriority(const GroupId& group_id,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override;

  void CancelRequest(const GroupId& group_id,
                     ClientSocketHandle* handle) override;
  void ReleaseSocket(const GroupId& group_id,
                     std::unique_ptr<StreamSocket> socket,
                     int id) override;
  void FlushWithError(int error) override;
  void CloseIdleSockets() override;
  void CloseIdleSocketsInGroup(const GroupId& group_id) override;
  int IdleSocketCount() const override;
  size_t IdleSocketCountInGroup(const GroupId& group_id) const override;
  LoadState GetLoadState(const GroupId& group_id,
                         const ClientSocketHandle* handle) const override;
  std::unique_ptr<base::DictionaryValue> GetInfoAsValue(
      const std::string& name,
      const std::string& type) const override;

  // LowerLayeredPool implementation.
  bool IsStalled() const override;
  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;
  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_dump_absolute_name) const;

  // Testing methods.

  size_t NumNeverAssignedConnectJobsInGroupForTesting(
      const GroupId& group_id) const {
    return base_.NumNeverAssignedConnectJobsInGroup(group_id);
  }

  size_t NumUnassignedConnectJobsInGroupForTesting(
      const GroupId& group_id) const {
    return base_.NumUnassignedConnectJobsInGroup(group_id);
  }

  size_t NumConnectJobsInGroupForTesting(const GroupId& group_id) const {
    return base_.NumConnectJobsInGroup(group_id);
  }

  int NumActiveSocketsInGroupForTesting(const GroupId& group_id) const {
    return base_.NumActiveSocketsInGroup(group_id);
  }

  bool RequestInGroupWithHandleHasJobForTesting(
      const GroupId& group_id,
      const ClientSocketHandle* handle) const {
    return base_.RequestInGroupWithHandleHasJobForTesting(group_id, handle);
  }

  bool HasGroupForTesting(const ClientSocketPool::GroupId& group_id) const {
    return base_.HasGroup(group_id);
  }

 protected:
  // Methods shared with WebSocketTransportClientSocketPool
  void NetLogTcpClientSocketPoolRequestedSocket(const NetLogWithSource& net_log,
                                                const GroupId& group_id);

 private:
  typedef ClientSocketPoolBase<SocketParams> PoolBase;

  TransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      std::unique_ptr<PoolBase::ConnectJobFactory> connect_job_factory,
      SSLConfigService* ssl_config_service,
      bool connect_backup_jobs_enabled);

  class TransportConnectJobFactory
      : public PoolBase::ConnectJobFactory {
   public:
    TransportConnectJobFactory(
        const CommonConnectJobParams* common_connect_job_params);
    ~TransportConnectJobFactory() override;

    // ClientSocketPoolBase::ConnectJobFactory methods.

    std::unique_ptr<ConnectJob> NewConnectJob(
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const override;

   private:
    const CommonConnectJobParams* common_connect_job_params_;

    DISALLOW_COPY_AND_ASSIGN(TransportConnectJobFactory);
  };

  // SSLConfigService::Observer methods.
  void OnSSLConfigChanged() override;

  PoolBase base_;
  SSLConfigService* const ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(TransportClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_H_
