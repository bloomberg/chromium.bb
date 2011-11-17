// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_stats.h"

#include "base/bind.h"
#include "base/callback_old.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/tuple.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/udp/udp_client_socket.h"
#include "net/udp/udp_server_socket.h"

using content::BrowserThread;

namespace chrome_browser_net {

// This specifies the number of bytes to be sent to the TCP/UDP servers as part
// of small packet size test.
static const uint32 kSmallTestBytesToSend = 100;

// This specifies the number of bytes to be sent to the TCP/UDP servers as part
// of large packet size test.
static const uint32 kLargeTestBytesToSend = 1200;

// This specifies the maximum message (payload) size.
static const uint32 kMaxMessage = 2048;

// This specifies starting position of the <version> and length of the
// <version> in "echo request" and "echo response".
static const uint32 kVersionNumber = 1;
static const uint32 kVersionStart = 0;
static const uint32 kVersionLength = 2;
static const uint32 kVersionEnd = kVersionStart + kVersionLength;

// This specifies the starting position of the <checksum> and length of the
// <checksum> in "echo request" and "echo response". Maximum value for the
// <checksum> is less than (2 ** 31 - 1).
static const uint32 kChecksumStart = kVersionEnd;
static const uint32 kChecksumLength = 10;
static const uint32 kChecksumEnd = kChecksumStart + kChecksumLength;

// This specifies the starting position of the <payload_size> and length of the
// <payload_size> in "echo request" and "echo response". Maximum number of bytes
// that can be sent in the <payload> is 9,999,999.
static const uint32 kPayloadSizeStart = kChecksumEnd;
static const uint32 kPayloadSizeLength = 7;
static const uint32 kPayloadSizeEnd = kPayloadSizeStart + kPayloadSizeLength;

// This specifies the starting position of the <key> and length of the <key> in
// "echo response".
static const uint32 kKeyStart = kPayloadSizeEnd;
static const uint32 kKeyLength = 6;
static const uint32 kKeyEnd = kKeyStart + kKeyLength;
static const int32 kKeyMinValue = 0;
static const int32 kKeyMaxValue = 999999;

// This specifies the starting position of the <payload> in "echo request".
static const uint32 kPayloadStart = kPayloadSizeEnd;

// This specifies the starting position of the <encoded_payload> and length of
// the <encoded_payload> in "echo response".
static const uint32 kEncodedPayloadStart = kKeyEnd;

// NetworkStats methods and members.
NetworkStats::NetworkStats()
    : load_size_(0),
      bytes_to_read_(0),
      bytes_to_send_(0),
      encoded_message_(""),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &NetworkStats::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &NetworkStats::OnWriteComplete)),
      finished_callback_(NULL),
      start_time_(base::TimeTicks::Now()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

NetworkStats::~NetworkStats() {
  socket_.reset();
}

bool NetworkStats::Start(net::HostResolver* host_resolver,
                         const net::HostPortPair& server_host_port_pair,
                         uint32 bytes_to_send,
                         net::OldCompletionCallback* finished_callback) {
  DCHECK(bytes_to_send);   // We should have data to send.

  Initialize(bytes_to_send, finished_callback);

  net::HostResolver::RequestInfo request(server_host_port_pair);
  int rv = host_resolver->Resolve(
      request, &addresses_,
      base::Bind(&NetworkStats::OnResolveComplete,
                 base::Unretained(this)),
      NULL, net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING)
    return true;
  return DoConnect(rv);
}

void NetworkStats::Initialize(uint32 bytes_to_send,
                              net::OldCompletionCallback* finished_callback) {
  DCHECK(bytes_to_send);   // We should have data to send.

  load_size_ = bytes_to_send;
  bytes_to_send_ = kVersionLength + kChecksumLength + kPayloadSizeLength +
      load_size_;
  bytes_to_read_ = kVersionLength + kChecksumLength + kPayloadSizeLength +
      kKeyLength + load_size_;
  finished_callback_ = finished_callback;
}

bool NetworkStats::ConnectComplete(int result) {
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

  // Timeout if we don't get response back from echo servers in 60 secs.
  const int kReadDataTimeoutMs = 60000;
  StartReadDataTimer(kReadDataTimeoutMs);

  ReadData();

  return true;
}

void NetworkStats::DoFinishCallback(int result) {
  if (finished_callback_ != NULL) {
    net::OldCompletionCallback* callback = finished_callback_;
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

  encoded_message_.append(read_buffer_->data(), result);

  read_buffer_ = NULL;
  bytes_to_read_ -= result;

  // No more data to read.
  if (!bytes_to_read_ || result == 0) {
    if (VerifyBytes())
      Finish(SUCCESS, net::OK);
    else
      Finish(READ_VERIFY_FAILED, net::ERR_INVALID_RESPONSE);
    return true;
  }
  return false;
}

void NetworkStats::OnResolveComplete(int result) {
  DoConnect(result);
}

void NetworkStats::OnReadComplete(int result) {
  if (!ReadComplete(result)) {
    // Called ReadData() via PostDelayedTask() to avoid recursion. Added a delay
    // of 1ms so that the time-out will fire before we have time to really hog
    // the CPU too extensively (waiting for the time-out) in case of an infinite
    // loop.
    const int kReadDataDelayMs = 1;
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NetworkStats::ReadData, weak_factory_.GetWeakPtr()),
        kReadDataDelayMs);
  }
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
  int rv;
  do {
    if (!socket_.get())
      return;

    DCHECK(!read_buffer_.get());

    // We release the read_buffer_ in the destructor if there is an error.
    read_buffer_ = new net::IOBuffer(kMaxMessage);

    rv = socket_->Read(read_buffer_, kMaxMessage, &read_callback_);
    if (rv == net::ERR_IO_PENDING)
      return;
    // If we have read all the data then return.
    if (ReadComplete(rv))
      return;
  } while (rv > 0);
}

int NetworkStats::SendData() {
  DCHECK(bytes_to_send_);   // We should have data to send.
  do {
    if (!write_buffer_.get()) {
      scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(bytes_to_send_));
      GetEchoRequest(buffer);
      write_buffer_ = new net::DrainableIOBuffer(buffer, bytes_to_send_);
    }

    if (!socket_.get())
      return net::ERR_UNEXPECTED;
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

void NetworkStats::StartReadDataTimer(int milliseconds) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NetworkStats::OnReadDataTimeout, weak_factory_.GetWeakPtr()),
      milliseconds);
}

void NetworkStats::OnReadDataTimeout() {
  Finish(READ_TIMED_OUT, net::ERR_INVALID_ARGUMENT);
}

void NetworkStats::GetEchoRequest(net::IOBuffer* io_buffer) {
  // Copy the <version> into the io_buffer starting from the kVersionStart
  // position.
  std::string version = base::StringPrintf("%02d", kVersionNumber);
  char* buffer = io_buffer->data() + kVersionStart;
  DCHECK(kVersionLength == version.length());
  memcpy(buffer, version.c_str(), kVersionLength);

  // Get the <payload> from the |stream_| and copy it into io_buffer starting
  // from the kPayloadStart position.
  buffer = io_buffer->data() + kPayloadStart;
  stream_.GetBytes(buffer, load_size_);

  // Calculate the <checksum> of the <payload>.
  uint32 sum = 0;
  for (uint32 i = 0; i < load_size_; ++i)
    sum += buffer[i];

  // Copy the <checksum> into the io_buffer starting from the kChecksumStart
  // position.
  std::string checksum = base::StringPrintf("%010d", sum);
  buffer = io_buffer->data() + kChecksumStart;
  DCHECK(kChecksumLength == checksum.length());
  memcpy(buffer, checksum.c_str(), kChecksumLength);

  // Copy the size of the <payload> into the io_buffer starting from the
  // kPayloadSizeStart position.
  buffer = io_buffer->data() + kPayloadSizeStart;
  std::string payload_size = base::StringPrintf("%07d", load_size_);
  DCHECK(kPayloadSizeLength == payload_size.length());
  memcpy(buffer, payload_size.c_str(), kPayloadSizeLength);
}

bool NetworkStats::VerifyBytes() {
  // If the "echo response" doesn't have enough bytes, then return false.
  if (encoded_message_.length() < kEncodedPayloadStart)
    return false;

  // Extract the |key| from the "echo response".
  std::string key_string = encoded_message_.substr(kKeyStart, kKeyLength);
  const char* key = key_string.c_str();
  int key_value = atoi(key);
  if (key_value < kKeyMinValue || key_value > kKeyMaxValue)
    return false;

  std::string encoded_payload =
      encoded_message_.substr(kEncodedPayloadStart);
  const char* encoded_data = encoded_payload.c_str();
  uint32 message_length = encoded_payload.length();
  message_length = std::min(message_length, kMaxMessage);
  // We should get back all the data we had sent.
  if (message_length != load_size_)
    return false;

  // Decrypt the data by looping through the |encoded_data| and XOR each byte
  // with the |key| to get the decoded byte. Append the decoded byte to the
  // |decoded_data|.
  char decoded_data[kMaxMessage + 1];
  for (uint32 data_index = 0, key_index = 0;
       data_index < message_length;
       ++data_index) {
    char encoded_byte = encoded_data[data_index];
    char key_byte = key[key_index];
    char decoded_byte = encoded_byte ^ key_byte;
    decoded_data[data_index] = decoded_byte;
    key_index = (key_index + 1) % kKeyLength;
  }

  return stream_.VerifyBytes(decoded_data, message_length);
}

// UDPStatsClient methods and members.
UDPStatsClient::UDPStatsClient()
    : NetworkStats() {
}

UDPStatsClient::~UDPStatsClient() {
}

bool UDPStatsClient::DoConnect(int result) {
  if (result != net::OK) {
    Finish(RESOLVE_FAILED, result);
    return false;
  }

  net::UDPClientSocket* udp_socket =
      new net::UDPClientSocket(net::DatagramSocket::DEFAULT_BIND,
                               net::RandIntCallback(),
                               NULL,
                               net::NetLog::Source());
  if (!udp_socket) {
    Finish(SOCKET_CREATE_FAILED, net::ERR_INVALID_ARGUMENT);
    return false;
  }
  set_socket(udp_socket);

  const addrinfo* address = GetAddressList().head();
  if (!address) {
    Finish(RESOLVE_FAILED, net::ERR_INVALID_ARGUMENT);
    return false;
  }

  net::IPEndPoint endpoint;
  endpoint.FromSockAddr(address->ai_addr, address->ai_addrlen);
  int rv = udp_socket->Connect(endpoint);
  if (rv < 0) {
    Finish(CONNECT_FAILED, rv);
    return false;
  }
  return NetworkStats::ConnectComplete(rv);
}

bool UDPStatsClient::ReadComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result <= 0) {
    Finish(READ_FAILED, result);
    return true;
  }
  return NetworkStats::ReadComplete(result);
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
          connect_callback_(this, &TCPStatsClient::OnConnectComplete)) {
}

TCPStatsClient::~TCPStatsClient() {
}

bool TCPStatsClient::DoConnect(int result) {
  if (result != net::OK) {
    Finish(RESOLVE_FAILED, result);
    return false;
  }

  net::TCPClientSocket* tcp_socket =
      new net::TCPClientSocket(GetAddressList(), NULL, net::NetLog::Source());
  if (!tcp_socket) {
    Finish(SOCKET_CREATE_FAILED, net::ERR_INVALID_ARGUMENT);
    return false;
  }
  set_socket(tcp_socket);

  int rv = tcp_socket->Connect(&connect_callback_);
  if (rv == net::ERR_IO_PENDING)
    return true;

  return NetworkStats::ConnectComplete(rv);
}

void TCPStatsClient::OnConnectComplete(int result) {
  NetworkStats::ConnectComplete(result);
}

bool TCPStatsClient::ReadComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result < 0) {
    Finish(READ_FAILED, result);
    return true;
  }
  return NetworkStats::ReadComplete(result);
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
        base::Bind(
            &CollectNetworkStats, network_stats_server, io_thread));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Check that there is a network connection. We get called only if UMA upload
  // to the server has succeeded.
  DCHECK(!net::NetworkChangeNotifier::IsOffline());

  CR_DEFINE_STATIC_LOCAL(scoped_refptr<base::FieldTrial>, trial, ());
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
  uint32 kTCPTestingPort = 8081;
  uint32 kUDPTestingPort = 6121;

  net::HostResolver* host_resolver = io_thread->globals()->host_resolver.get();
  DCHECK(host_resolver);

  net::HostPortPair udp_server_address(network_stats_server, kUDPTestingPort);

  UDPStatsClient* small_udp_stats = new UDPStatsClient();
  small_udp_stats->Start(
      host_resolver, udp_server_address, kSmallTestBytesToSend, NULL);

  UDPStatsClient* large_udp_stats = new UDPStatsClient();
  large_udp_stats->Start(
      host_resolver, udp_server_address, kLargeTestBytesToSend, NULL);

  net::HostPortPair tcp_server_address(network_stats_server, kTCPTestingPort);

  TCPStatsClient* small_tcp_client = new TCPStatsClient();
  small_tcp_client->Start(
      host_resolver, tcp_server_address, kSmallTestBytesToSend, NULL);

  TCPStatsClient* large_tcp_client = new TCPStatsClient();
  large_tcp_client->Start(
      host_resolver, tcp_server_address, kLargeTestBytesToSend, NULL);
}

}  // namespace chrome_browser_net
