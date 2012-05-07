// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_stats.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/tuple.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
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

// This specifies the length of the packet_number in the payload.
static const uint32 kPacketNumberLength = 10;

// This specifies the starting position of the <encoded_payload> in
// "echo response".
static const uint32 kEncodedPayloadStart = kKeyEnd;

// HistogramPortSelector and kPorts should be kept in sync.
static const int32 kPorts[] = {53, 80, 587, 6121, 8080, 999999};

// The packet number that is to be sent to the server.
static uint32 g_packet_number_ = 0;

// Maximum number of packets that can be sent to the server for packet loss
// testing.
static const uint32 kMaximumPackets = 4;

// NetworkStats methods and members.
NetworkStats::NetworkStats()
    : load_size_(0),
      bytes_to_read_(0),
      bytes_to_send_(0),
      packets_to_send_(0),
      packets_sent_(0),
      base_packet_number_(0),
      packets_received_mask_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

NetworkStats::~NetworkStats() {
  socket_.reset();
}

bool NetworkStats::Start(net::HostResolver* host_resolver,
                         const net::HostPortPair& server_host_port_pair,
                         HistogramPortSelector histogram_port,
                         uint32 bytes_to_send,
                         uint32 packets_to_send,
                         const net::CompletionCallback& finished_callback) {
  DCHECK(bytes_to_send);   // We should have data to send.
  DCHECK_LE(packets_to_send, kMaximumPackets);

  Initialize(bytes_to_send, histogram_port, packets_to_send, finished_callback);

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

void NetworkStats::Initialize(
    uint32 bytes_to_send,
    HistogramPortSelector histogram_port,
    uint32 packets_to_send,
    const net::CompletionCallback& finished_callback) {
  DCHECK(bytes_to_send);    // We should have data to send.
  DCHECK(packets_to_send);  // We should send at least 1 packet.
  DCHECK_LE(bytes_to_send, kLargeTestBytesToSend);

  load_size_ = bytes_to_send;
  packets_to_send_ = packets_to_send;

  histogram_port_ = histogram_port;
  finished_callback_ = finished_callback;
}

uint32 SendingPacketSize(uint32 load_size) {
  return kVersionLength + kChecksumLength + kPayloadSizeLength + load_size;
}

uint32 ReceivingPacketSize(uint32 load_size) {
  return kVersionLength + kChecksumLength + kPayloadSizeLength + kKeyLength +
      load_size;
}

bool NetworkStats::ConnectComplete(int result) {
  if (result < 0) {
    Finish(CONNECT_FAILED, result);
    return false;
  }

  base_packet_number_ = g_packet_number_;
  start_time_ = base::TimeTicks::Now();
  SendPacket();
  return true;
}

void NetworkStats::DoFinishCallback(int result) {
  if (!finished_callback_.is_null()) {
    net::CompletionCallback callback = finished_callback_;
    finished_callback_.Reset();
    callback.Run(result);
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
    Status status = VerifyPackets();
    if (status == SUCCESS)
      Finish(status, net::OK);
    else
      Finish(status, net::ERR_INVALID_RESPONSE);
    return true;
  }
  return false;
}

void NetworkStats::OnResolveComplete(int result) {
  DoConnect(result);
}

void NetworkStats::SendPacket() {
  uint32 sending_packet_size = SendingPacketSize(load_size_);
  uint32 receiving_packet_size = ReceivingPacketSize(load_size_);

  while (packets_to_send_ > packets_sent_) {
    ++g_packet_number_;

    bytes_to_send_ = sending_packet_size;
    int rv = SendData();
    if (rv < 0) {
      if (rv != net::ERR_IO_PENDING) {
        Finish(WRITE_FAILED, rv);
        return;
      }
    }
    bytes_to_read_ += receiving_packet_size;
    ++packets_sent_;

    DCHECK(bytes_to_send_ == 0 || rv == net::ERR_IO_PENDING);
    if (rv == net::ERR_IO_PENDING)
      return;
  }

  // Timeout if we don't get response back from echo servers in 60 secs.
  const int kReadDataTimeoutMs = 60000;
  StartReadDataTimer(kReadDataTimeoutMs);

  ReadData();
}

void NetworkStats::OnReadComplete(int result) {
  if (!ReadComplete(result)) {
    // Called ReadData() via PostDelayedTask() to avoid recursion. Added a delay
    // of 1ms so that the time-out will fire before we have time to really hog
    // the CPU too extensively (waiting for the time-out) in case of an infinite
    // loop.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NetworkStats::ReadData, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(1));
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
    return;
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&NetworkStats::SendPacket, weak_factory_.GetWeakPtr()));
}

void NetworkStats::ReadData() {
  int rv;
  do {
    if (!socket_.get())
      return;

    DCHECK(!read_buffer_.get());

    // We release the read_buffer_ in the destructor if there is an error.
    read_buffer_ = new net::IOBuffer(kMaxMessage);

    rv = socket_->Read(read_buffer_, kMaxMessage,
                       base::Bind(&NetworkStats::OnReadComplete,
                                  base::Unretained(this)));
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
      scoped_refptr<net::IOBufferWithSize> buffer(
          new net::IOBufferWithSize(bytes_to_send_));
      GetEchoRequest(buffer);
      write_buffer_ = new net::DrainableIOBuffer(buffer, bytes_to_send_);
    }

    if (!socket_.get())
      return net::ERR_UNEXPECTED;
    int rv = socket_->Write(write_buffer_,
                            write_buffer_->BytesRemaining(),
                            base::Bind(&NetworkStats::OnWriteComplete,
                                       base::Unretained(this)));
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
      base::TimeDelta::FromMilliseconds(milliseconds));
}

void NetworkStats::OnReadDataTimeout() {
  Status status = VerifyPackets();
  if (status == SUCCESS)
    Finish(status, net::OK);
  else
    Finish(READ_TIMED_OUT, net::ERR_INVALID_ARGUMENT);
}

uint32 NetworkStats::GetChecksum(const char* message, uint32 message_length) {
  // Calculate the <checksum> of the <message>.
  uint32 sum = 0;
  for (uint32 i = 0; i < message_length; ++i)
    sum += message[i];
  return sum;
}

void NetworkStats::Crypt(const char* key,
                         uint32 key_length,
                         const char* data,
                         uint32 data_length,
                         char* encoded_data) {
  // Decrypt the data by looping through the |data| and XOR each byte with the
  // |key| to get the decoded byte. Append the decoded byte to the
  // |encoded_data|.
  for (uint32 data_index = 0, key_index = 0;
       data_index < data_length;
       ++data_index) {
    char data_byte = data[data_index];
    char key_byte = key[key_index];
    char encoded_byte = data_byte ^ key_byte;
    encoded_data[data_index] = encoded_byte;
    key_index = (key_index + 1) % key_length;
  }
}

void NetworkStats::GetEchoRequest(net::IOBufferWithSize* io_buffer) {
  char* buffer = io_buffer->data();
  uint32 buffer_size = static_cast<uint32>(io_buffer->size());

  // Copy the <version> into the io_buffer starting from the kVersionStart
  // position.
  std::string version = base::StringPrintf("%02d", kVersionNumber);
  DCHECK(kVersionLength == version.length());
  DCHECK_GE(buffer_size, kVersionStart + kVersionLength);
  memcpy(buffer + kVersionStart, version.c_str(), kVersionLength);

  // Copy the packet_number into the payload.
  std::string packet_number = base::StringPrintf("%010d", g_packet_number_);
  DCHECK(kPacketNumberLength == packet_number.length());
  DCHECK_GE(buffer_size, kPayloadStart + kPacketNumberLength);
  memcpy(buffer + kPayloadStart, packet_number.c_str(), kPacketNumberLength);

  // Get the <payload> from the |stream_| and copy it into io_buffer after
  // packet_number.
  stream_.Reset();
  DCHECK_GE(buffer_size, kPayloadStart + load_size_);
  stream_.GetBytes(buffer + kPayloadStart + kPacketNumberLength,
                   load_size_ - kPacketNumberLength);

  // Calculate the <checksum> of the <payload>.
  uint32 sum = GetChecksum(buffer + kPayloadStart, load_size_);

  // Copy the <checksum> into the io_buffer starting from the kChecksumStart
  // position.
  std::string checksum = base::StringPrintf("%010d", sum);
  DCHECK(kChecksumLength == checksum.length());
  DCHECK_GE(buffer_size, kChecksumStart + kChecksumLength);
  memcpy(buffer + kChecksumStart, checksum.c_str(), kChecksumLength);

  // Copy the size of the <payload> into the io_buffer starting from the
  // kPayloadSizeStart position.
  std::string payload_size = base::StringPrintf("%07d", load_size_);
  DCHECK(kPayloadSizeLength == payload_size.length());
  DCHECK_GE(buffer_size, kPayloadSizeStart + kPayloadSizeLength);
  memcpy(buffer + kPayloadSizeStart, payload_size.c_str(), kPayloadSizeLength);
}

NetworkStats::Status NetworkStats::VerifyPackets() {
  uint32 packet_start = 0;
  size_t message_length = encoded_message_.length();
  uint32 receiving_packet_size = ReceivingPacketSize(load_size_);
  Status status = SUCCESS;
  for (uint32 i = 0; i < packets_to_send_; i++) {
    if (message_length <= packet_start) {
      status = ZERO_LENGTH_ERROR;
      break;
    }
    std::string response =
        encoded_message_.substr(packet_start, receiving_packet_size);
    Status packet_status = VerifyBytes(response);
    if (packet_status != SUCCESS)
      status = packet_status;
    packet_start += receiving_packet_size;
  }
  if (message_length > packet_start)
    status = TOO_MANY_PACKETS;
  return status;
}

NetworkStats::Status NetworkStats::VerifyBytes(const std::string& response) {
  // If the "echo response" doesn't have enough bytes, then return false.
  if (response.length() <= kVersionStart)
    return ZERO_LENGTH_ERROR;
  if (response.length() <= kChecksumStart)
    return NO_CHECKSUM_ERROR;
  if (response.length() <= kPayloadSizeStart)
    return NO_PAYLOAD_SIZE_ERROR;
  if (response.length() <= kKeyStart)
    return NO_KEY_ERROR;
  if (response.length() <= kEncodedPayloadStart)
    return NO_PAYLOAD_ERROR;

  // Extract the |key| from the "echo response".
  std::string key_string = response.substr(kKeyStart, kKeyLength);
  const char* key = key_string.c_str();
  int key_value = atoi(key);
  if (key_value < kKeyMinValue || key_value > kKeyMaxValue)
    return INVALID_KEY_ERROR;

  std::string encoded_payload = response.substr(kEncodedPayloadStart);
  const char* encoded_data = encoded_payload.c_str();
  uint32 message_length = encoded_payload.length();
  message_length = std::min(message_length, kMaxMessage);
  if (message_length < load_size_)
    return TOO_SHORT_PAYLOAD;
  if (message_length > load_size_)
    return TOO_LONG_PAYLOAD;

  // Decode/decrypt the |encoded_data| into |decoded_data|.
  char decoded_data[kMaxMessage + 1];
  DCHECK_LE(message_length, kMaxMessage);
  memset(decoded_data, 0, kMaxMessage + 1);
  Crypt(key, kKeyLength, encoded_data, message_length, decoded_data);

  // Calculate the <checksum> of the <decoded_data>.
  uint32 sum = GetChecksum(decoded_data, message_length);
  // Extract the |checksum| from the "echo response".
  std::string checksum_string =
      response.substr(kChecksumStart, kChecksumLength);
  const char* checksum = checksum_string.c_str();
  uint32 checksum_value = atoi(checksum);
  if (checksum_value != sum)
    return INVALID_CHECKSUM;

  // Verify the packet_number.
  char packet_number_data[kPacketNumberLength + 1];
  memset(packet_number_data, 0, kPacketNumberLength + 1);
  memcpy(packet_number_data, decoded_data, kPacketNumberLength);
  uint32 packet_number = atoi(packet_number_data);
  uint32 packet_index = packet_number - base_packet_number_;
  if (packet_index > packets_to_send_)
    return INVALID_PACKET_NUMBER;

  stream_.Reset();
  if (!stream_.VerifyBytes(&decoded_data[kPacketNumberLength],
                           message_length - kPacketNumberLength)) {
    return PATTERN_CHANGED;
  }

  packets_received_mask_ |= 1 << (packet_index - 1);
  return SUCCESS;
}

// static
void NetworkStats::GetHistogramNames(const ProtocolValue& protocol,
                                     HistogramPortSelector port,
                                     uint32 load_size,
                                     int result,
                                     std::string* rtt_histogram_name,
                                     std::string* status_histogram_name,
                                     std::string* packet_loss_histogram_name) {
  CHECK_GE(port, PORT_53);
  CHECK_LE(port, HISTOGRAM_PORT_MAX);

  // Build <protocol> string.
  const char* kTcpString = "TCP";
  const char* kUdpString = "UDP";
  const char* protocol_string;
  if (protocol == PROTOCOL_TCP)
    protocol_string = kTcpString;
  else
    protocol_string = kUdpString;

  // Build <load_size> string.
  const char* kSmallLoadString = "100B";
  const char* kLargeLoadString = "1K";
  const char* load_size_string;
  if (load_size == kSmallTestBytesToSend)
    load_size_string = kSmallLoadString;
  else
    load_size_string = kLargeLoadString;

  // Build "NetConnectivity.<protocol>.Success.<port>.<load_size>.RTT"
  // histogram name. Total number of histograms are 2*5*2.
  if (result == net::OK) {
    *rtt_histogram_name = base::StringPrintf(
        "NetConnectivity.%s.Success.%d.%s.RTT",
        protocol_string,
        kPorts[port],
        load_size_string);
  }

  // Build "NetConnectivity.<protocol>.Status.<port>.<load_size>" histogram
  // name. Total number of histograms are 2*5*2.
  *status_histogram_name =  base::StringPrintf(
      "NetConnectivity.%s.Status.%d.%s",
      protocol_string,
      kPorts[port],
      load_size_string);

  // Build "NetConnectivity.<protocol>.PacketLoss.<port>.<load_size>" histogram
  // name. Total number of histograms are 5 (because we do this test for UDP
  // only).
  *packet_loss_histogram_name =  base::StringPrintf(
      "NetConnectivity.%s.PacketLoss.%d.%s",
      protocol_string,
      kPorts[port],
      load_size_string);
}

void NetworkStats::RecordHistograms(const ProtocolValue& protocol,
                                    const Status& status,
                                    int result) {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time();

  std::string rtt_histogram_name;
  std::string status_histogram_name;
  std::string packet_loss_histogram_name;
  GetHistogramNames(protocol,
                    histogram_port_,
                    load_size_,
                    result,
                    &rtt_histogram_name,
                    &status_histogram_name,
                    &packet_loss_histogram_name);

  // For packet loss test, just record packet loss data.
  if (packets_to_send_ > 1) {
    base::Histogram* packet_loss_histogram = base::LinearHistogram::FactoryGet(
        packet_loss_histogram_name,
        1,
        2 << kMaximumPackets,
        (2 << kMaximumPackets) + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    packet_loss_histogram->Add(packets_received_mask_);
    return;
  }

  if (result == net::OK) {
    base::Histogram* rtt_histogram = base::Histogram::FactoryTimeGet(
        rtt_histogram_name,
        base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(60), 50,
        base::Histogram::kUmaTargetedHistogramFlag);
    rtt_histogram->AddTime(duration);
  }

  base::Histogram* status_histogram = base::LinearHistogram::FactoryGet(
      status_histogram_name, 1, STATUS_MAX, STATUS_MAX+1,
      base::Histogram::kUmaTargetedHistogramFlag);
  status_histogram->Add(status);
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

  if (addresses().empty()) {
    Finish(RESOLVE_FAILED, net::ERR_INVALID_ARGUMENT);
    return false;
  }

  const net::IPEndPoint& endpoint = addresses().front();
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
  RecordHistograms(PROTOCOL_UDP, status, result);

  DoFinishCallback(result);

  // Close the socket so that there are no more IO operations.
  net::UDPClientSocket* udp_socket =
      static_cast<net::UDPClientSocket*>(socket());
  if (udp_socket)
    udp_socket->Close();

  delete this;
}

// TCPStatsClient methods and members.
TCPStatsClient::TCPStatsClient() {
}

TCPStatsClient::~TCPStatsClient() {
}

bool TCPStatsClient::DoConnect(int result) {
  if (result != net::OK) {
    Finish(RESOLVE_FAILED, result);
    return false;
  }

  net::TCPClientSocket* tcp_socket =
      new net::TCPClientSocket(addresses(), NULL, net::NetLog::Source());
  if (!tcp_socket) {
    Finish(SOCKET_CREATE_FAILED, net::ERR_INVALID_ARGUMENT);
    return false;
  }
  set_socket(tcp_socket);

  int rv = tcp_socket->Connect(base::Bind(&TCPStatsClient::OnConnectComplete,
                                          base::Unretained(this)));
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
  RecordHistograms(PROTOCOL_TCP, status, result);

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

  static uint32 port;
  static NetworkStats::HistogramPortSelector histogram_port;

  if (!trial.get()) {
    // Set up a field trial to collect network stats for UDP and TCP.
    const base::FieldTrial::Probability kDivisor = 1000;

    // Enable the connectivity testing for 0.5% of the users in stable channel.
    base::FieldTrial::Probability probability_per_group = 5;

    chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
    if (channel == chrome::VersionInfo::CHANNEL_CANARY)
      probability_per_group = kDivisor;
    else if (channel == chrome::VersionInfo::CHANNEL_DEV)
      // Enable the connectivity testing for 10% of the users in dev channel.
      probability_per_group = 500;
    else if (channel == chrome::VersionInfo::CHANNEL_BETA)
      // Enable the connectivity testing for 5% of the users in beta channel.
      probability_per_group = 50;

    // After October 30, 2012 builds, it will always be in default group
    // (disable_network_stats).
    trial = base::FieldTrialList::FactoryGetFieldTrial(
        "NetworkConnectivity", kDivisor, "disable_network_stats",
        2012, 10, 30, NULL);

    // Add option to collect_stats for NetworkConnectivity.
    int collect_stats_group = trial->AppendGroup("collect_stats",
                                                 probability_per_group);
    if (trial->group() == collect_stats_group)
      collect_stats = true;

    if (collect_stats) {
      // Pick a port randomly from the set of TCP/UDP echo server ports
      // specified in |kPorts|.
      histogram_port = static_cast<NetworkStats::HistogramPortSelector>(
          base::RandInt(NetworkStats::PORT_53, NetworkStats::PORT_8080));
      DCHECK_GE(histogram_port, NetworkStats::PORT_53);
      DCHECK_LE(histogram_port, NetworkStats::PORT_8080);
      port = kPorts[histogram_port];
    }
  }

  if (!collect_stats)
    return;

  // Run test kMaxNumberOfTests times.
  const size_t kMaxNumberOfTests = INT_MAX;
  static size_t number_of_tests_done = 0;
  if (number_of_tests_done > kMaxNumberOfTests)
    return;

  ++number_of_tests_done;

  net::HostResolver* host_resolver = io_thread->globals()->host_resolver.get();
  DCHECK(host_resolver);

  net::HostPortPair server_address(network_stats_server, port);

  int experiment_to_run = base::RandInt(1, 5);
  switch (experiment_to_run) {
    case 1:
      {
        UDPStatsClient* small_udp_stats = new UDPStatsClient();
        small_udp_stats->Start(
            host_resolver, server_address, histogram_port,
            kSmallTestBytesToSend, 1, net::CompletionCallback());
      }
      break;

    case 2:
      {
        UDPStatsClient* large_udp_stats = new UDPStatsClient();
        large_udp_stats->Start(
            host_resolver, server_address, histogram_port,
            kLargeTestBytesToSend, 1, net::CompletionCallback());
      }
      break;

    case 3:
      {
        TCPStatsClient* small_tcp_client = new TCPStatsClient();
        small_tcp_client->Start(
            host_resolver, server_address, histogram_port,
            kSmallTestBytesToSend, 1, net::CompletionCallback());
      }
      break;

    case 4:
      {
        TCPStatsClient* large_tcp_client = new TCPStatsClient();
        large_tcp_client->Start(
            host_resolver, server_address, histogram_port,
            kLargeTestBytesToSend, 1, net::CompletionCallback());
      }
      break;

    case 5:
      {
        UDPStatsClient* packet_loss_udp_stats = new UDPStatsClient();
        packet_loss_udp_stats->Start(
            host_resolver, server_address, histogram_port,
            kLargeTestBytesToSend, kMaximumPackets, net::CompletionCallback());
      }
      break;
  }
}

}  // namespace chrome_browser_net
