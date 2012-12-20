// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SESSION_H_
#define NET_DNS_DNS_SESSION_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_socket_pool.h"

namespace net {

class ClientSocketFactory;
class DatagramClientSocket;
class NetLog;
class StreamSocket;

// Session parameters and state shared between DNS transactions.
// Ref-counted so that DnsClient::Request can keep working in absence of
// DnsClient. A DnsSession must be recreated when DnsConfig changes.
class NET_EXPORT_PRIVATE DnsSession
    : NON_EXPORTED_BASE(public base::RefCounted<DnsSession>) {
 public:
  typedef base::Callback<int()> RandCallback;

  class NET_EXPORT_PRIVATE SocketLease {
   public:
    SocketLease(scoped_refptr<DnsSession> session,
                unsigned server_index,
                scoped_ptr<DatagramClientSocket> socket);
    ~SocketLease();

    unsigned server_index() const { return server_index_; }

    DatagramClientSocket* socket() { return socket_.get(); }

   private:
    scoped_refptr<DnsSession> session_;
    unsigned server_index_;
    scoped_ptr<DatagramClientSocket> socket_;

    DISALLOW_COPY_AND_ASSIGN(SocketLease);
  };

  DnsSession(const DnsConfig& config,
             scoped_ptr<DnsSocketPool> socket_pool,
             const RandIntCallback& rand_int_callback,
             NetLog* net_log);

  const DnsConfig& config() const { return config_; }
  NetLog* net_log() const { return net_log_; }

  // Return the next random query ID.
  int NextQueryId() const;

  // Return the index of the first configured server to use on first attempt.
  int NextFirstServerIndex();

  // Return the timeout for the next query.
  base::TimeDelta NextTimeout(int attempt);

  // Allocate a socket, already connected to the server address.
  // When the SocketLease is destroyed, the socket will be freed.
  scoped_ptr<SocketLease> AllocateSocket(unsigned server_index,
                                         const NetLog::Source& source);

  // Creates a StreamSocket from the factory for a transaction over TCP. These
  // sockets are not pooled.
  scoped_ptr<StreamSocket> CreateTCPSocket(unsigned server_index,
                                           const NetLog::Source& source);

 private:
  friend class base::RefCounted<DnsSession>;
  ~DnsSession();

  // Release a socket.
  void FreeSocket(unsigned server_index,
                  scoped_ptr<DatagramClientSocket> socket);

  const DnsConfig config_;
  scoped_ptr<DnsSocketPool> socket_pool_;
  RandCallback rand_callback_;
  NetLog* net_log_;

  // Current index into |config_.nameservers| to begin resolution with.
  int server_index_;

  // TODO(szym): Add current RTT estimate.
  // TODO(szym): Add TCP connection pool to support DNS over TCP.

  DISALLOW_COPY_AND_ASSIGN(DnsSession);
};

}  // namespace net

#endif  // NET_DNS_DNS_SESSION_H_
