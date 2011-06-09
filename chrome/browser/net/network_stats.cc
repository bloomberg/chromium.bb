// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_stats.h"

#include "base/callback_old.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/tuple.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/udp/udp_client_socket.h"
#include "net/udp/udp_server_socket.h"

namespace chrome_browser_net {

// This specifies the number of bytes to be sent to the TCP/UDP servers as part
// of small packet size test.
static const int kSmallTestBytesToSend = 100;

// This specifies the number of bytes to be sent to the TCP/UDP servers as part
// of large packet size test.
static const int kLargeTestBytesToSend = 1200;

// NetworkStats methods and members.
NetworkStats::NetworkStats()
    : bytes_to_read_(0),
      bytes_to_send_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &NetworkStats::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &NetworkStats::OnWriteComplete)),
      finished_callback_(NULL),
      start_time_(base::TimeTicks::Now()) {
}

NetworkStats::~NetworkStats() {
  socket_.reset();
}

void NetworkStats::Initialize(int bytes_to_send,
                              net::CompletionCallback* finished_callback) {
  DCHECK(bytes_to_send);   // We should have data to send.

  load_size_ = bytes_to_send;
  bytes_to_send_ = bytes_to_send;
  bytes_to_read_ = bytes_to_send;
  finished_callback_ = finished_callback;
}

bool NetworkStats::DoStart(int result) {
  if (result < 0) {
    Finish(CONNECT_FAILED, result);
    return false;
  }

  DCHECK(bytes_to_send_);   // We should have data to send.

  start_time_ = base::TimeTicks::Now();

  int rv = SendData();
  if (rv < 0) {
    if (rv != net::ERR_IO_PENDING) {
      Finish(WRITE_FAILED, rv);
      return false;
    }
  }

  stream_.Reset();
  ReadData();

  return true;
}

void NetworkStats::DoFinishCallback(int result) {
  if (finished_callback_ != NULL) {
    net::CompletionCallback* callback = finished_callback_;
    finished_callback_ = NULL;
    callback->Run(result);
  }
}

void NetworkStats::set_socket(net::Socket* socket) {
  DCHECK(socket);
  DCHECK(!socket_.get());
  socket_.reset(socket);
}

bool NetworkStats::ReadComplete(int result) {
  DCHECK(socket_.get());
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result < 0) {
    Finish(READ_FAILED, result);
    return true;
  }

  if (!stream_.VerifyBytes(read_buffer_->data(), result)) {
    Finish(READ_VERIFY_FAILED, net::ERR_INVALID_RESPONSE);
    return true;
  }

  read_buffer_ = NULL;
  bytes_to_read_ -= result;

  // No more data to read.
  if (!bytes_to_read_) {
    Finish(SUCCESS, net::OK);
    return true;
  }
  ReadData();
  return false;
}

void NetworkStats::OnReadComplete(int result) {
  ReadComplete(result);
}

void NetworkStats::OnWriteComplete(int result) {
  DCHECK(socket_.get());
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result < 0) {
    Finish(WRITE_FAILED, result);
    return;
  }

  write_buffer_->DidConsume(result);
  bytes_to_send_ -= result;
  if (!write_buffer_->BytesRemaining())
      write_buffer_ = NULL;

  if (bytes_to_send_) {
    int rv = SendData();
    if (rv < 0) {
      if (rv != net::ERR_IO_PENDING) {
        Finish(WRITE_FAILED, rv);
        return;
      }
    }
  }
}

void NetworkStats::ReadData() {
  DCHECK(!read_buffer_.get());
  int kMaxMessage = 2048;

  // We release the read_buffer_ in the destructor if there is an error.
  read_buffer_ = new net::IOBuffer(kMaxMessage);

  int rv;
  do {
    DCHECK(socket_.get());
    rv = socket_->Read(read_buffer_, kMaxMessage, &read_callback_);
    if (rv == net::ERR_IO_PENDING)
      return;
    if (ReadComplete(rv))  // Complete the read manually.
      return;
  } while (rv > 0);
}

int NetworkStats::SendData() {
  DCHECK(bytes_to_send_);   // We should have data to send.
  do {
    if (!write_buffer_.get()) {
      scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(bytes_to_send_));
      stream_.GetBytes(buffer->data(), bytes_to_send_);
      write_buffer_ = new net::DrainableIOBuffer(buffer, bytes_to_send_);
    }

    DCHECK(socket_.get());
    int rv = socket_->Write(write_buffer_,
                            write_buffer_->BytesRemaining(),
                            &write_callback_);
    if (rv < 0)
      return rv;
    write_buffer_->DidConsume(rv);
    bytes_to_send_ -= rv;
    if (!write_buffer_->BytesRemaining())
      write_buffer_ = NULL;
  } while (bytes_to_send_);
  return net::OK;
}

// UDPStatsClient methods and members.
UDPStatsClient::UDPStatsClient()
    : NetworkStats() {
}

UDPStatsClient::~UDPStatsClient() {
}

bool UDPStatsClient::Start(const std::string& ip_str,
                           int port,
                           int bytes_to_send,
                           net::CompletionCallback* finished_callback) {
  DCHECK(port);
  DCHECK(bytes_to_send);   // We should have data to send.

  Initialize(bytes_to_send, finished_callback);

  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_str, &ip_number)) {
    Finish(IP_STRING_PARSE_FAILED, net::ERR_INVALID_ARGUMENT);
    return false;
  }
  net::IPEndPoint server_address = net::IPEndPoint(ip_number, port);

  net::UDPClientSocket* udp_socket =
     new net::UDPClientSocket(NULL, net::NetLog::Source());
  DCHECK(udp_socket);
  set_socket(udp_socket);

  int rv = udp_socket->Connect(server_address);
  return DoStart(rv);
}

void UDPStatsClient::Finish(Status status, int result) {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time();
  if (load_size() == kSmallTestBytesToSend) {
    if (result == net::OK)
      UMA_HISTOGRAM_TIMES("NetConnectivity.UDP.Success.100B.RTT", duration);
    else
      UMA_HISTOGRAM_TIMES("NetConnectivity.UDP.Fail.100B.RTT", duration);

    UMA_HISTOGRAM_ENUMERATION(
        "NetConnectivity.UDP.Status.100B", status, STATUS_MAX);
  } else {
    if (result == net::OK)
      UMA_HISTOGRAM_TIMES("NetConnectivity.UDP.Success.1K.RTT", duration);
    else
      UMA_HISTOGRAM_TIMES("NetConnectivity.UDP.Fail.1K.RTT", duration);

    UMA_HISTOGRAM_ENUMERATION(
        "NetConnectivity.UDP.Status.1K", status, STATUS_MAX);
  }

  DoFinishCallback(result);

  // Close the socket so that there are no more IO operations.
  net::UDPClientSocket* udp_socket =
      static_cast<net::UDPClientSocket*>(socket());
  if (udp_socket)
    udp_socket->Close();

  delete this;
}

// TCPStatsClient methods and members.
TCPStatsClient::TCPStatsClient()
  : NetworkStats(),
    ALLOW_THIS_IN_INITIALIZER_LIST(
        resolve_callback_(this, &TCPStatsClient::OnResolveComplete)),
    ALLOW_THIS_IN_INITIALIZER_LIST(
        connect_callback_(this, &TCPStatsClient::OnConnectComplete)) {
}

TCPStatsClient::~TCPStatsClient() {
}

bool TCPStatsClient::Start(net::HostResolver* host_resolver,
                           const net::HostPortPair& server_host_port_pair,
                           int bytes_to_send,
                           net::CompletionCallback* finished_callback) {
  DCHECK(bytes_to_send);   // We should have data to send.

  Initialize(bytes_to_send, finished_callback);

  net::HostResolver::RequestInfo request(server_host_port_pair);
  int rv = host_resolver->Resolve(request,
                                  &addresses_,
                                  &resolve_callback_,
                                  NULL,
                                  net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    return true;
  return DoConnect(rv);
}

void TCPStatsClient::OnResolveComplete(int result) {
  DoConnect(result);
}

bool TCPStatsClient::DoConnect(int result) {
  if (result != net::OK) {
    Finish(RESOLVE_FAILED, result);
    return false;
  }

  net::TCPClientSocket* tcp_socket =
      new net::TCPClientSocket(addresses_, NULL, net::NetLog::Source());
  DCHECK(tcp_socket);
  set_socket(tcp_socket);

  int rv = tcp_socket->Connect(&connect_callback_);
  if (rv == net::ERR_IO_PENDING)
    return true;

  return DoStart(rv);
}

void TCPStatsClient::OnConnectComplete(int result) {
  DoStart(result);
}

void TCPStatsClient::Finish(Status status, int result) {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time();
  if (load_size() == kSmallTestBytesToSend) {
    if (result == net::OK)
      UMA_HISTOGRAM_TIMES("NetConnectivity.TCP.Success.100B.RTT", duration);
    else
      UMA_HISTOGRAM_TIMES("NetConnectivity.TCP.Fail.100B.RTT", duration);

    UMA_HISTOGRAM_ENUMERATION(
        "NetConnectivity.TCP.Status.100B", status, STATUS_MAX);
  } else {
    if (result == net::OK)
      UMA_HISTOGRAM_TIMES("NetConnectivity.TCP.Success.1K.RTT", duration);
    else
      UMA_HISTOGRAM_TIMES("NetConnectivity.TCP.Fail.1K.RTT", duration);

    UMA_HISTOGRAM_ENUMERATION(
        "NetConnectivity.TCP.Status.1K", status, STATUS_MAX);
  }

  DoFinishCallback(result);

  // Disconnect the socket so that there are no more IO operations.
  net::TCPClientSocket* tcp_socket =
      static_cast<net::TCPClientSocket*>(socket());
  if (tcp_socket)
    tcp_socket->Disconnect();

  delete this;
}

// static
void CollectNetworkStats(const std::string& network_stats_server,
                         IOThread* io_thread) {
  if (network_stats_server.empty())
    return;

  // If we are not on IO Thread, then post a task to call CollectNetworkStats on
  // IO Thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableFunction(
            &CollectNetworkStats, network_stats_server, io_thread));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Check that there is a network connection. We get called only if UMA upload
  // to the server has succeeded.
  DCHECK(!net::NetworkChangeNotifier::IsOffline());

  static scoped_refptr<base::FieldTrial> trial = NULL;
  static bool collect_stats = false;

  if (!trial.get()) {
    // Set up a field trial to collect network stats for UDP and TCP.
    base::FieldTrial::Probability kDivisor = 1000;

    // Enable the connectivity testing for 0.5% of the users.
    base::FieldTrial::Probability kProbabilityPerGroup = 5;

    // After October 30, 2011 builds, it will always be in default group
    // (disable_network_stats).
    trial = new base::FieldTrial("NetworkConnectivity", kDivisor,
                                 "disable_network_stats", 2011, 10, 30);

    // Add option to collect_stats for NetworkConnectivity.
    int collect_stats_group = trial->AppendGroup("collect_stats",
                                                 kProbabilityPerGroup);
    if (trial->group() == collect_stats_group)
      collect_stats = true;
  }

  if (!collect_stats)
    return;

  // Run test kMaxNumberOfTests times.
  const size_t kMaxNumberOfTests = INT_MAX;
  static size_t number_of_tests_done = 0;
  if (number_of_tests_done > kMaxNumberOfTests)
    return;

  ++number_of_tests_done;

  // Use SPDY's UDP port per http://www.iana.org/assignments/port-numbers.
  // |network_stats_server| echo TCP and UDP servers listen on the following
  // ports.
  uint32 kTCPTestingPort = 80;
  uint32 kUDPTestingPort = 6121;

  UDPStatsClient* small_udp_stats = new UDPStatsClient();
  small_udp_stats->Start(
      network_stats_server, kUDPTestingPort, kSmallTestBytesToSend, NULL);

  UDPStatsClient* large_udp_stats = new UDPStatsClient();
  large_udp_stats->Start(
      network_stats_server, kUDPTestingPort, kLargeTestBytesToSend, NULL);

  net::HostResolver* host_resolver = io_thread->globals()->host_resolver.get();
  DCHECK(host_resolver);

  net::HostPortPair server_address(network_stats_server, kTCPTestingPort);

  TCPStatsClient* small_tcp_client = new TCPStatsClient();
  small_tcp_client->Start(host_resolver, server_address, kSmallTestBytesToSend,
                          NULL);

  TCPStatsClient* large_tcp_client = new TCPStatsClient();
  large_tcp_client->Start(host_resolver, server_address, kLargeTestBytesToSend,
                          NULL);
}

}  // namespace chrome_browser_net
