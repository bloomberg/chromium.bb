// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_H_
#define NET_SOCKET_CONNECT_JOB_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_tag.h"

namespace net {

class ClientSocketHandle;
class StreamSocket;

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

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // A |timeout_duration| of 0 corresponds to no timeout. |group_name| is a
  // caller-provided opaque string, only used for logging and the corresponding
  // accessor.
  ConnectJob(const std::string& group_name,
             base::TimeDelta timeout_duration,
             RequestPriority priority,
             const SocketTag& socket_tag,
             bool respect_limits,
             Delegate* delegate,
             const NetLogWithSource& net_log);
  virtual ~ConnectJob();

  // Accessors
  const std::string& group_name() const { return group_name_; }
  const NetLogWithSource& net_log() { return net_log_; }
  RequestPriority priority() const { return priority_; }
  bool respect_limits() const { return respect_limits_; }

  // Releases ownership of the underlying socket to the caller. Returns the
  // released socket, or nullptr if there was a connection error.
  std::unique_ptr<StreamSocket> PassSocket();

  void ChangePriority(RequestPriority priority);

  // Begins connecting the socket.  Returns OK on success, ERR_IO_PENDING if it
  // cannot complete synchronously without blocking, or another net error code
  // on error.  In asynchronous completion, the ConnectJob will notify
  // |delegate_| via OnConnectJobComplete.  In both asynchronous and synchronous
  // completion, ReleaseSocket() can be called to acquire the connected socket
  // if it succeeded.
  //
  // On completion, the ConnectJob must be completed synchronously, since it
  // doesn't bother to stop its timer when complete.
  int Connect();

  virtual LoadState GetLoadState() const = 0;

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
  const SocketTag& socket_tag() const { return socket_tag_; }
  void SetSocket(std::unique_ptr<StreamSocket> socket);
  StreamSocket* socket() { return socket_.get(); }
  void NotifyDelegateOfCompletion(int rv);
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

  const std::string group_name_;
  const base::TimeDelta timeout_duration_;
  RequestPriority priority_;
  const SocketTag socket_tag_;
  const bool respect_limits_;
  // Timer to abort jobs that take too long.
  base::OneShotTimer timer_;
  Delegate* delegate_;
  std::unique_ptr<StreamSocket> socket_;
  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(ConnectJob);
};

}  // namespace net

#endif  // NET_SOCKET_CONNECT_JOB_H_
