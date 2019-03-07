// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_H_
#define NET_SOCKET_CONNECT_JOB_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_tag.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class ClientSocketFactory;
class ClientSocketHandle;
class HostResolver;
class HttpAuthController;
class HttpResponseInfo;
class NetLog;
class NetworkQualityEstimator;
class ProxyDelegate;
class SocketPerformanceWatcherFactory;
class StreamSocket;
class WebSocketEndpointLockManager;

// Immutable socket parameters intended for shared use by all ConnectJob types.
// Excludes priority because it can be modified over the lifetime of a
// ConnectJob. Excludes connection timeout and NetLogWithSource because
// ConnectJobs that wrap other ConnectJobs typically have different values for
// those.
struct NET_EXPORT_PRIVATE CommonConnectJobParams {
  CommonConnectJobParams(
      const SocketTag& socket_tag,
      ClientSocketFactory* client_socket_factory,
      HostResolver* host_resolver,
      ProxyDelegate* proxy_delegate,
      const SSLClientSocketContext& ssl_client_socket_context,
      const SSLClientSocketContext& ssl_client_socket_context_privacy_mode,
      SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager);
  CommonConnectJobParams(const CommonConnectJobParams& other);
  ~CommonConnectJobParams();

  CommonConnectJobParams& operator=(const CommonConnectJobParams& other);

  // Tag applied to any created socket.
  SocketTag socket_tag;

  ClientSocketFactory* client_socket_factory;
  HostResolver* host_resolver;
  ProxyDelegate* proxy_delegate;
  SSLClientSocketContext ssl_client_socket_context;
  SSLClientSocketContext ssl_client_socket_context_privacy_mode;
  SocketPerformanceWatcherFactory* socket_performance_watcher_factory;
  NetworkQualityEstimator* network_quality_estimator;
  NetLog* net_log;

  // This must only be non-null for WebSockets.
  WebSocketEndpointLockManager* websocket_endpoint_lock_manager;
};

// ConnectJob provides an abstract interface for "connecting" a socket.
// The connection may involve host resolution, tcp connection, ssl connection,
// etc.
class NET_EXPORT_PRIVATE ConnectJob {
 public:
  // Alerts the delegate that the connection completed. |job| must be destroyed
  // by the delegate. A std::unique_ptr<> isn't used because the caller of this
  // function doesn't own |job|.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Alerts the delegate that the connection completed. |job| must be
    // destroyed by the delegate. A std::unique_ptr<> isn't used because the
    // caller of this function doesn't own |job|.
    virtual void OnConnectJobComplete(int result, ConnectJob* job) = 0;

    // Invoked when an HTTP proxy returns an HTTP auth challenge during tunnel
    // establishment. Always invoked asynchronously. The caller should use
    // |auth_controller| to set challenge response information and then invoke
    // |restart_with_auth_callback| to continue establishing a connection, or
    // delete the ConnectJob if it doesn't want to respond to the challenge.
    //
    // Will only be called once at a time. Neither OnConnectJobComplete() nor
    // OnNeedsProxyAuth() will be called synchronously when
    // |restart_with_auth_callback| is invoked. Will not be called after
    // OnConnectJobComplete() has been invoked.
    virtual void OnNeedsProxyAuth(const HttpResponseInfo& response,
                                  HttpAuthController* auth_controller,
                                  base::OnceClosure restart_with_auth_callback,
                                  ConnectJob* job) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // A |timeout_duration| of 0 corresponds to no timeout.
  //
  // If |net_log| is non-NULL, the ConnectJob will use it for logging.
  // Otherwise, a new one will be created of type |net_log_source_type|.
  //
  // |net_log_connect_event_type| is the NetLog event type logged on Connect()
  // and connect completion.
  ConnectJob(RequestPriority priority,
             base::TimeDelta timeout_duration,
             const CommonConnectJobParams& common_connect_job_params,
             Delegate* delegate,
             const NetLogWithSource* net_log,
             NetLogSourceType net_log_source_type,
             NetLogEventType net_log_connect_event_type);
  virtual ~ConnectJob();

  // Accessors
  const NetLogWithSource& net_log() { return net_log_; }
  RequestPriority priority() const { return priority_; }

  // Releases ownership of the underlying socket to the caller. Returns the
  // released socket, or nullptr if there was a connection error.
  std::unique_ptr<StreamSocket> PassSocket();

  // Returns the connected socket, or nullptr if PassSocket() has already been
  // called. Used to query the socket state. May only be called after the
  // ConnectJob completes.
  StreamSocket* socket() { return socket_.get(); }

  void ChangePriority(RequestPriority priority);

  // Begins connecting the socket.  Returns OK on success, ERR_IO_PENDING if it
  // cannot complete synchronously without blocking, or another net error code
  // on error.  In asynchronous completion, the ConnectJob will notify
  // |delegate_| via OnConnectJobComplete.  In both asynchronous and synchronous
  // completion, ReleaseSocket() can be called to acquire the connected socket
  // if it succeeded.
  //
  // On completion, the ConnectJob must be destroyed synchronously, since it
  // doesn't bother to stop its timer when complete.
  // TODO(mmenke): Can that be fixed?
  int Connect();

  // Returns the current LoadState of the ConnectJob. Each ConnectJob class must
  // start (optionally) with a LOAD_STATE_RESOLVING_HOST followed by
  // LOAD_STATE_CONNECTING, and never return to LOAD_STATE_CONNECTING. This
  // behavior is needed for backup ConnectJobs to function correctly.
  //
  // TODO(mmenke): Can something better be done here?
  virtual LoadState GetLoadState() const = 0;

  // Returns true if the ConnectJob has ever successfully established a TCP
  // connection. Used solely for deciding if a backup job is needed. Once it
  // starts returning true, must always return true when called in the future.
  // Not safe to call after NotifyComplete() is invoked.
  virtual bool HasEstablishedConnection() const = 0;

  // If Connect returns an error (or OnConnectJobComplete reports an error
  // result) this method will be called, allowing a SocketPool to add additional
  // error state to the ClientSocketHandle (post late-binding).
  //
  // TODO(mmenke): This is a layering violation. Consider refactoring it to not
  // depend on ClientSocketHandle. Fixing this will need to wait until after
  // proxy tunnel auth has been refactored.
  virtual void GetAdditionalErrorState(ClientSocketHandle* handle) {}

  const LoadTimingInfo::ConnectTiming& connect_timing() const {
    return connect_timing_;
  }

  const NetLogWithSource& net_log() const { return net_log_; }

 protected:
  const SocketTag& socket_tag() const {
    return common_connect_job_params_.socket_tag;
  }
  ClientSocketFactory* client_socket_factory() {
    return common_connect_job_params_.client_socket_factory;
  }
  HostResolver* host_resolver() {
    return common_connect_job_params_.host_resolver;
  }
  const SSLClientSocketContext& ssl_client_socket_context() {
    return common_connect_job_params_.ssl_client_socket_context;
  }
  const SSLClientSocketContext& ssl_client_socket_context_privacy_mode() {
    return common_connect_job_params_.ssl_client_socket_context_privacy_mode;
  }
  SocketPerformanceWatcherFactory* socket_performance_watcher_factory() {
    return common_connect_job_params_.socket_performance_watcher_factory;
  }
  NetworkQualityEstimator* network_quality_estimator() {
    return common_connect_job_params_.network_quality_estimator;
  }
  WebSocketEndpointLockManager* websocket_endpoint_lock_manager() {
    return common_connect_job_params_.websocket_endpoint_lock_manager;
  }
  const CommonConnectJobParams& common_connect_job_params() {
    return common_connect_job_params_;
  }

  void SetSocket(std::unique_ptr<StreamSocket> socket);
  void NotifyDelegateOfCompletion(int rv);
  void NotifyDelegateOfProxyAuth(const HttpResponseInfo& response,
                                 HttpAuthController* auth_controller,
                                 base::OnceClosure restart_with_auth_callback);
  void ResetTimer(base::TimeDelta remaining_time);

  // Connection establishment timing information.
  // TODO(mmenke): This should be private.
  LoadTimingInfo::ConnectTiming connect_timing_;

 private:
  virtual int ConnectInternal() = 0;

  virtual void ChangePriorityInternal(RequestPriority priority) = 0;

  void LogConnectStart();
  void LogConnectCompletion(int net_error);

  // Alerts the delegate that the ConnectJob has timed out.
  void OnTimeout();

  const base::TimeDelta timeout_duration_;
  RequestPriority priority_;
  const CommonConnectJobParams common_connect_job_params_;
  // Timer to abort jobs that take too long.
  base::OneShotTimer timer_;
  Delegate* delegate_;
  std::unique_ptr<StreamSocket> socket_;
  // Indicates if this is the topmost ConnectJob. The topmost ConnectJob logs an
  // extra begin and end event, to allow callers to log extra data before the
  // ConnectJob has started / after it has completed.
  const bool top_level_job_;
  NetLogWithSource net_log_;
  const NetLogEventType net_log_connect_event_type_;

  DISALLOW_COPY_AND_ASSIGN(ConnectJob);
};

}  // namespace net

#endif  // NET_SOCKET_CONNECT_JOB_H_
