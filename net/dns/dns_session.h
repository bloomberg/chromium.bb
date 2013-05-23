// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SESSION_H_
#define NET_DNS_DNS_SESSION_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_socket_pool.h"

namespace base {
class BucketRanges;
class SampleVector;
}

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

  // Record how long it took to receive a response from the server.
  void RecordRTT(unsigned server_index, base::TimeDelta rtt);

  // Record suspected loss of a packet for a specific server.
  void RecordLostPacket(unsigned server_index, int attempt);

  // Return the timeout for the next query. |attempt| counts from 0 and is used
  // for exponential backoff.
  base::TimeDelta NextTimeout(unsigned server_index, int attempt);

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

  // Return the timeout using the TCP timeout method.
  base::TimeDelta NextTimeoutFromJacobson(unsigned server_index, int attempt);

  // Compute the timeout using the histogram method.
  base::TimeDelta NextTimeoutFromHistogram(unsigned server_index, int attempt);

  const DnsConfig config_;
  scoped_ptr<DnsSocketPool> socket_pool_;
  RandCallback rand_callback_;
  NetLog* net_log_;

  // Current index into |config_.nameservers| to begin resolution with.
  int server_index_;

  // Estimated RTT for each name server using moving average.
  std::vector<base::TimeDelta> rtt_estimates_;
  // Estimated error in the above for each name server.
  std::vector<base::TimeDelta> rtt_deviations_;

  // Buckets shared for all |rtt_histograms_|.
  scoped_ptr<base::BucketRanges> rtt_buckets_;
  // A histogram of observed RTT for each name server.
  ScopedVector<base::SampleVector> rtt_histograms_;

  DISALLOW_COPY_AND_ASSIGN(DnsSession);
};

}  // namespace net

#endif  // NET_DNS_DNS_SESSION_H_
