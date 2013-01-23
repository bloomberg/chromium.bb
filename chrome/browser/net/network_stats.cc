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
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "net/proxy/proxy_service.h"
#include "net/udp/udp_client_socket.h"
#include "net/udp/udp_server_socket.h"

using content::BrowserThread;

namespace chrome_browser_net {

// This specifies the number of bytes to be sent to the UDP echo servers as part
// of small packet size test.
static const uint32 kSmallTestBytesToSend = 100;

// This specifies the number of bytes to be sent to the UDP echo servers as part
// of medium packet size test.
static const uint32 kMediumTestBytesToSend = 500;

// This specifies the number of bytes to be sent to the UDP echo servers as part
// of large packet size test.
static const uint32 kLargeTestBytesToSend = 1200;

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

// HistogramPortSelector and kPorts should be kept in sync. Port 999999 is
// used by the unit tests.
static const int32 kPorts[] = {6121, 999999};

// Number of packets that are recorded in a packet-correlation histogram, which
// shows exactly what sequence of packets were responded to.  We use this to
// deduce specific packet loss correlation.
static const uint32 kCorrelatedLossPacketCount = 6;

// Maximum number of packets that can be sent to the server.
static const uint32 kMaximumSequentialPackets = 21;

// This specifies the maximum message (payload) size.
static const uint32 kMaxMessage = kMaximumSequentialPackets * 2048;

// NetworkStats methods and members.
NetworkStats::NetworkStats()
    : read_buffer_(NULL),
      write_buffer_(NULL),
      load_size_(0),
      bytes_to_read_(0),
      bytes_to_send_(0),
      has_proxy_server_(false),
      packets_to_send_(0),
      packets_sent_(0),
      packets_received_(0),
      packets_received_mask_(0),
      packet_number_(0),
      base_packet_number_(0),
      sending_complete_(false),
      current_test_(START_PACKET_TEST),
      next_test_(TEST_TYPE_MAX),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

NetworkStats::~NetworkStats() {
  socket_.reset();
}

bool NetworkStats::Start(net::HostResolver* host_resolver,
                         const net::HostPortPair& server_host_port_pair,
                         HistogramPortSelector histogram_port,
                         bool has_proxy_server,
                         uint32 bytes_to_send,
                         uint32 packets_to_send,
                         const net::CompletionCallback& finished_callback) {
  DCHECK(host_resolver);
  DCHECK(bytes_to_send);   // We should have data to send.
  DCHECK_LE(packets_to_send, kMaximumSequentialPackets);

  Initialize(bytes_to_send,
             histogram_port,
             has_proxy_server,
             packets_to_send,
             finished_callback);

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

void NetworkStats::RestartPacketTest() {
  ResetData();
  current_test_ = next_test_;
  next_test_ = TEST_TYPE_MAX;
  if (!bytes_to_read_) {
    read_buffer_ = NULL;
    ReadData();
  }
  SendPacket();
}

void NetworkStats::Initialize(
    uint32 bytes_to_send,
    HistogramPortSelector histogram_port,
    bool has_proxy_server,
    uint32 packets_to_send,
    const net::CompletionCallback& finished_callback) {
  DCHECK(bytes_to_send);    // We should have data to send.
  DCHECK(packets_to_send);  // We should send at least 1 packet.
  DCHECK_LE(bytes_to_send, kLargeTestBytesToSend);
  DCHECK_LE(packets_to_send,  8 * sizeof(packets_received_mask_));

  load_size_ = bytes_to_send;
  packets_to_send_ = packets_to_send;
  histogram_port_ = histogram_port;
  has_proxy_server_ = has_proxy_server;
  finished_callback_ = finished_callback;
  ResetData();
  packet_number_ = base::RandInt(1 << 28, INT_MAX);
}

void NetworkStats::ResetData() {
  write_buffer_ = NULL;
  bytes_to_send_ = 0;
  packet_status_.clear();
  packet_status_.resize(packets_to_send_);
  packets_sent_ = 0;
  packets_received_ = 0;
  packets_received_mask_ = 0;
  sending_complete_ = false;
}

void NetworkStats::OnResolveComplete(int result) {
  DoConnect(result);
}

bool NetworkStats::DoConnect(int result) {
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

  const int kSocketBufferSize = 2 * packets_to_send_ * 2048;
  udp_socket->SetSendBufferSize(kSocketBufferSize);
  udp_socket->SetReceiveBufferSize(kSocketBufferSize);
  return ConnectComplete(rv);
}

bool NetworkStats::ConnectComplete(int result) {
  if (result < 0) {
    Finish(CONNECT_FAILED, result);
    return false;
  }

  ReadData();
  SendPacket();
  return true;
}

void NetworkStats::SendPacket() {
  while (true) {
    if (bytes_to_send_ == 0u) {
      if (packets_sent_ >= packets_to_send_) {
        // Timeout if we don't get response back from echo servers in 30 secs.
        sending_complete_ = true;
        const int kReadDataTimeoutMs = 30000;
        StartReadDataTimer(kReadDataTimeoutMs);
        break;
      }

      ++packet_number_;
      if (packets_sent_ == 0)
        base_packet_number_ = packet_number_;
      bytes_to_send_ = SendingPacketSize();
      SendNextPacketAfterDelay();
      break;
    }

    int rv = SendData();
    if (rv < 0) {
      if (rv != net::ERR_IO_PENDING)
        Finish(WRITE_FAILED, rv);
      break;
    }
    DCHECK_EQ(bytes_to_send_, 0u);
  };
}

void NetworkStats::SendNextPacketAfterDelay() {
  if (current_test_ == PACED_PACKET_TEST) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&NetworkStats::SendPacket, weak_factory_.GetWeakPtr()),
        average_time_);
    return;
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&NetworkStats::SendPacket, weak_factory_.GetWeakPtr()));
}

bool NetworkStats::ReadComplete(int result) {
  DCHECK(socket_.get());
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result < 0) {
    Finish(READ_FAILED, result);
    return true;
  }

  if (result > 0) {
    std::string encoded_message;
    encoded_message.append(read_buffer_->data(), result);
    if (VerifyBytes(encoded_message) == SUCCESS) {
      base::TimeTicks now = base::TimeTicks::Now();
      if (packets_received_ == 0)
        packet_1st_byte_read_time_ = now;
      packet_last_byte_read_time_ = now;

      DCHECK_GE(bytes_to_read_, static_cast<uint32>(result));
      if (bytes_to_read_ >= static_cast<uint32>(result))
        bytes_to_read_ -= result;
      ++packets_received_;
    }
  }

  read_buffer_ = NULL;

  // No more data to read.
  if (!bytes_to_read_ || result == 0) {
    if (!sending_complete_)
      return false;

    Status status = VerifyPackets();
    if (status == SUCCESS)
      Finish(status, net::OK);
    else
      Finish(status, net::ERR_INVALID_RESPONSE);
    return true;
  }
  return false;
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

  DidSendData(result);
  if (bytes_to_send_) {
    int rv = SendData();
    if (rv < 0) {
      if (rv != net::ERR_IO_PENDING)
        Finish(WRITE_FAILED, rv);
      return;
    }
    DCHECK_EQ(rv, net::OK);
    DCHECK_EQ(bytes_to_send_, 0u);
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&NetworkStats::SendPacket, weak_factory_.GetWeakPtr()));
}

void NetworkStats::ReadData() {
  int rv;
  do {
    if (!socket_.get())
      break;

    DCHECK(!read_buffer_.get());

    // We release the read_buffer_ in the destructor if there is an error.
    read_buffer_ = new net::IOBuffer(kMaxMessage);

    rv = socket_->Read(read_buffer_, kMaxMessage,
                       base::Bind(&NetworkStats::OnReadComplete,
                                  base::Unretained(this)));
    if (rv == net::ERR_IO_PENDING)
      break;

    // If we have read all the data then return.
    if (ReadComplete(rv))
      break;
  } while (rv > 0);
}

int NetworkStats::SendData() {
  DCHECK(bytes_to_send_);   // We should have data to send.
  do {
    if (!write_buffer_.get()) {
      // Send a new packet.
      scoped_refptr<net::IOBufferWithSize> buffer(
          new net::IOBufferWithSize(bytes_to_send_));
      GetEchoRequest(buffer);
      write_buffer_ = new net::DrainableIOBuffer(buffer, bytes_to_send_);

      // As soon as we write, a read could happen. Thus update all the book
      // keeping data.
      bytes_to_read_ += ReceivingPacketSize();
      ++packets_sent_;
      if (packets_sent_ >= packets_to_send_)
        sending_complete_ = true;

      uint32 packet_index = packet_number_ - base_packet_number_;
      DCHECK_GE(packet_index, 0u);
      DCHECK_LT(packet_index, packet_status_.size());
      packet_status_[packet_index].start_time_ = base::TimeTicks::Now();
    }

    if (!socket_.get())
      return net::ERR_UNEXPECTED;
    int rv = socket_->Write(write_buffer_,
                            write_buffer_->BytesRemaining(),
                            base::Bind(&NetworkStats::OnWriteComplete,
                                       base::Unretained(this)));
    if (rv < 0)
      return rv;
    DidSendData(rv);
  } while (bytes_to_send_);
  return net::OK;
}

uint32 NetworkStats::SendingPacketSize() const {
  return kVersionLength + kChecksumLength + kPayloadSizeLength + load_size_;
}

uint32 NetworkStats::ReceivingPacketSize() const {
  return kVersionLength + kChecksumLength + kPayloadSizeLength + kKeyLength +
      load_size_;
}

void NetworkStats::DidSendData(int bytes_sent) {
  write_buffer_->DidConsume(bytes_sent);
  if (!write_buffer_->BytesRemaining())
    write_buffer_ = NULL;
  bytes_to_send_ -= bytes_sent;
}

void NetworkStats::StartReadDataTimer(int milliseconds) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NetworkStats::OnReadDataTimeout,
                 weak_factory_.GetWeakPtr(),
                 base_packet_number_),
      base::TimeDelta::FromMilliseconds(milliseconds));
}

void NetworkStats::OnReadDataTimeout(uint32 test_base_packet_number) {
  if (test_base_packet_number != base_packet_number_)
    return;

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
  std::string packet_number = base::StringPrintf("%010d", packet_number_);
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
  Status status = SUCCESS;
  uint32 successful_packets = 0;

  for (uint32 i = 0; i < packet_status_.size(); i++) {
    if (packets_received_mask_ & (1 << i))
      ++successful_packets;
  }

  if (packets_received_ > packets_to_send_)
    status = TOO_MANY_PACKETS;

  if (packets_to_send_ > successful_packets)
    status = SOME_PACKETS_NOT_VERIFIED;

  if (packets_to_send_ == kMaximumSequentialPackets &&
      successful_packets > 1) {
    base::TimeDelta total_time;
    if (packet_last_byte_read_time_ > packet_1st_byte_read_time_) {
      total_time =
          packet_last_byte_read_time_ - packet_1st_byte_read_time_;
    }
    average_time_ = total_time / (successful_packets - 1);
    std::string histogram_name = base::StringPrintf(
        "NetConnectivity3.%s.Sent%02d.%d.%dB.PacketDelay",
        TestName(),
        kMaximumSequentialPackets,
        kPorts[histogram_port_],
        load_size_);
    base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
        histogram_name, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromSeconds(30), 50,
        base::Histogram::kUmaTargetedHistogramFlag);
    histogram->AddTime(total_time);

    if (current_test_ == START_PACKET_TEST) {
        int experiment_to_run = base::RandInt(1, 2);
        if (experiment_to_run == 1)
          next_test_ = NON_PACED_PACKET_TEST;
        else
          next_test_ = PACED_PACKET_TEST;
    }
  }

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
  uint32 packet_number_received = atoi(packet_number_data);
  if (packet_number_received < base_packet_number_)
    return PREVIOUS_PACKET_NUMBER;
  uint32 packet_index = packet_number_received - base_packet_number_;
  if (packet_index >= packets_to_send_)
    return INVALID_PACKET_NUMBER;

  stream_.Reset();
  if (!stream_.VerifyBytes(&decoded_data[kPacketNumberLength],
                           message_length - kPacketNumberLength)) {
    return PATTERN_CHANGED;
  }

  if (packets_received_mask_ & (1 << packet_index))
    return DUPLICATE_PACKET;

  packets_received_mask_ |= 1 << packet_index;
  DCHECK_GE(packet_index, 0u);
  DCHECK_LT(packet_index, packet_status_.size());
  packet_status_[packet_index].end_time_ = base::TimeTicks::Now();
  return SUCCESS;
}

void NetworkStats::Finish(Status status, int result) {
  // Set the base_packet_number_ for the start of next test. Changing the
  // |base_packet_number_| indicates to OnReadDataTimeout that the Finish has
  // already been called for the test and that it doesn't need to call Finish
  // again.
  base_packet_number_ = packet_number_ + 1;
  RecordHistograms(PROTOCOL_UDP, status, result);

  if (next_test() == NON_PACED_PACKET_TEST ||
      next_test() == PACED_PACKET_TEST) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NetworkStats::RestartPacketTest,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  DoFinishCallback(result);

  // Close the socket so that there are no more IO operations.
  net::UDPClientSocket* udp_socket =
      static_cast<net::UDPClientSocket*>(socket());
  if (udp_socket)
    udp_socket->Close();

  delete this;
}

void NetworkStats::DoFinishCallback(int result) {
  if (!finished_callback_.is_null()) {
    net::CompletionCallback callback = finished_callback_;
    finished_callback_.Reset();
    callback.Run(result);
  }
}

void NetworkStats::RecordHistograms(const ProtocolValue& protocol,
                                    const Status& status,
                                    int result) {
  if (packets_to_send_ != kMaximumSequentialPackets)
    return;

  std::string load_size_string = base::StringPrintf("%dB", load_size_);

  RecordPacketLossSeriesHistograms(protocol, load_size_string, status, result);

  for (uint32 i = 0; i < 3; i++)
    RecordRTTHistograms(protocol, load_size_string, i);

  RecordRTTHistograms(protocol, load_size_string, 9);
  RecordRTTHistograms(protocol, load_size_string, 19);

  RecordAcksReceivedHistograms(load_size_string);
}

void NetworkStats::RecordAcksReceivedHistograms(
    const std::string& load_size_string) {
  DCHECK_EQ(packets_to_send_, kMaximumSequentialPackets);

  const char* test_name = TestName();
  bool received_atleast_one_packet = packets_received_mask_ > 0;

  std::string histogram_name = base::StringPrintf(
      "NetConnectivity3.%s.Sent%02d.GotAnAck.%d.%s",
      test_name,
      kMaximumSequentialPackets,
      kPorts[histogram_port_],
      load_size_string.c_str());
  base::HistogramBase* got_an_ack_histogram =
      base::BooleanHistogram::FactoryGet(
          histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  got_an_ack_histogram->AddBoolean(received_atleast_one_packet);

  histogram_name = base::StringPrintf(
      "NetConnectivity3.%s.Sent%02d.PacketsSent.%d.%s",
      test_name,
      kMaximumSequentialPackets,
      kPorts[histogram_port_],
      load_size_string.c_str());
  base::HistogramBase* packets_sent_histogram =
      base::Histogram::FactoryGet(
          histogram_name,
          1, kMaximumSequentialPackets, kMaximumSequentialPackets + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  packets_sent_histogram->Add(packets_sent_);

  if (!received_atleast_one_packet || packets_sent_ != packets_to_send_)
    return;

  histogram_name = base::StringPrintf(
      "NetConnectivity3.%s.Sent%02d.AckReceivedForNthPacket.%02d.%s",
      test_name,
      kMaximumSequentialPackets,
      kPorts[histogram_port_],
      load_size_string.c_str());
  base::HistogramBase* ack_received_for_nth_packet_histogram =
      base::Histogram::FactoryGet(
          histogram_name,
          1, kMaximumSequentialPackets + 1, kMaximumSequentialPackets + 2,
          base::HistogramBase::kUmaTargetedHistogramFlag);

  int count = 0;
  for (size_t j = 0; j < packets_to_send_; j++) {
    int packet_number = j + 1;
    if (packets_received_mask_ & (1 << j)) {
      ack_received_for_nth_packet_histogram->Add(packet_number);
      count++;
    }
    if (packet_number < 2)
      continue;
    histogram_name = base::StringPrintf(
        "NetConnectivity3.%s.Sent%02d.AcksReceivedFromFirst%02dPackets.%d.%s",
        test_name,
        kMaximumSequentialPackets,
        packet_number,
        kPorts[histogram_port_],
        load_size_string.c_str());
    base::HistogramBase* acks_received_count_histogram =
        base::Histogram::FactoryGet(
            histogram_name, 1, packet_number, packet_number + 1,
            base::HistogramBase::kUmaTargetedHistogramFlag);
    acks_received_count_histogram->Add(count);
  }
}

void NetworkStats::RecordPacketLossSeriesHistograms(
    const ProtocolValue& protocol,
    const std::string& load_size_string,
    const Status& status,
    int result) {
  DCHECK_GT(packets_to_send_, kCorrelatedLossPacketCount);
  const char* test_name = TestName();

  // Build "NetConnectivity3.Send6.SeriesAcked.<port>.<load_size>" histogram
  // name. Total number of histograms are 5*2.
  std::string series_acked_histogram_name = base::StringPrintf(
      "NetConnectivity3.%s.Send6.SeriesAcked.%d.%s",
      test_name,
      kPorts[histogram_port_],
      load_size_string.c_str());

  uint32 correlated_packet_mask =
    ((1 << kCorrelatedLossPacketCount) - 1) & packets_received_mask_;

  // If we are running without a proxy, we'll generate 2 distinct histograms in
  // each case, one will have the ".NoProxy" suffix.
  size_t histogram_count = has_proxy_server_ ? 1 : 2;
  for (size_t i = 0; i < histogram_count; i++) {
    // For packet loss test, just record packet loss data.
    base::HistogramBase* series_acked_histogram =
        base::LinearHistogram::FactoryGet(
             series_acked_histogram_name,
             1,
             1 << kCorrelatedLossPacketCount,
             (1 << kCorrelatedLossPacketCount) + 1,
             base::HistogramBase::kUmaTargetedHistogramFlag);
    series_acked_histogram->Add(correlated_packet_mask);
    series_acked_histogram_name.append(".NoProxy");
  }
}

void NetworkStats::RecordRTTHistograms(const ProtocolValue& protocol,
                                       const std::string& load_size_string,
                                       uint32 index) {
  DCHECK_GE(index, 0u);
  DCHECK_LT(index, packet_status_.size());

  const char* test_name = TestName();
  std::string rtt_histogram_name = base::StringPrintf(
      "NetConnectivity3.%s.Sent%02d.Success.RTT.Packet%02d.%d.%s",
      test_name,
      packets_to_send_,
      index + 1,
      kPorts[histogram_port_],
      load_size_string.c_str());
  base::HistogramBase* rtt_histogram = base::Histogram::FactoryTimeGet(
      rtt_histogram_name,
      base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromSeconds(30), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  base::TimeDelta duration =
      packet_status_[index].end_time_ - packet_status_[index].start_time_;
  rtt_histogram->AddTime(duration);
}

const char* NetworkStats::TestName() {
  switch (current_test_) {
    case START_PACKET_TEST:
      return "StartPacket";
    case NON_PACED_PACKET_TEST:
      return "NonPacedPacket";
    case PACED_PACKET_TEST:
      return "PacedPacket";
    default:
      NOTREACHED();
      return "None";
  }
}

void NetworkStats::set_socket(net::Socket* socket) {
  DCHECK(socket);
  DCHECK(!socket_.get());
  socket_.reset(socket);
}

// ProxyDetector methods and members.
ProxyDetector::ProxyDetector(net::ProxyService* proxy_service,
                             const net::HostPortPair& server_address,
                             OnResolvedCallback callback)
    : proxy_service_(proxy_service),
      server_address_(server_address),
      callback_(callback),
      has_pending_proxy_resolution_(false) {
}

ProxyDetector::~ProxyDetector() {
  CHECK(!has_pending_proxy_resolution_);
}

void ProxyDetector::StartResolveProxy() {
  std::string url =
      base::StringPrintf("https://%s", server_address_.ToString().c_str());
  GURL gurl(url);

  has_pending_proxy_resolution_ = true;
  DCHECK(proxy_service_);
  int rv = proxy_service_->ResolveProxy(
      gurl,
      &proxy_info_,
      base::Bind(&ProxyDetector::OnResolveProxyComplete,
                 base::Unretained(this)),
      NULL,
      net::BoundNetLog());
  if (rv != net::ERR_IO_PENDING)
    OnResolveProxyComplete(rv);
}

void ProxyDetector::OnResolveProxyComplete(int result) {
  has_pending_proxy_resolution_ = false;
  bool has_proxy_server = (result == net::OK &&
                           proxy_info_.proxy_server().is_valid() &&
                           !proxy_info_.proxy_server().is_direct());

  OnResolvedCallback callback = callback_;
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, has_proxy_server));

  // TODO(rtenneti): Will we leak if ProxyResolve is cancelled (or proxy
  // resolution never completes).
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
  static NetworkStats::HistogramPortSelector histogram_port =
      NetworkStats::PORT_6121;

  if (!trial.get()) {
    // Set up a field trial to collect network stats for UDP.
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

  net::HostPortPair server_address(network_stats_server,
                                   kPorts[histogram_port]);

  net::ProxyService* proxy_service =
      io_thread->globals()->system_proxy_service.get();
  DCHECK(proxy_service);

  ProxyDetector::OnResolvedCallback callback =
      base::Bind(&StartNetworkStatsTest,
          host_resolver, server_address, histogram_port);

  ProxyDetector* proxy_client = new ProxyDetector(
      proxy_service, server_address, callback);
  proxy_client->StartResolveProxy();
}

// static
void StartNetworkStatsTest(net::HostResolver* host_resolver,
                           const net::HostPortPair& server_address,
                           NetworkStats::HistogramPortSelector histogram_port,
                           bool has_proxy_server) {
  int experiment_to_run = base::RandInt(1, 3);
  switch (experiment_to_run) {
    case 1:
      {
        NetworkStats* udp_stats_client = new NetworkStats();
        udp_stats_client->Start(
            host_resolver, server_address, histogram_port, has_proxy_server,
            kSmallTestBytesToSend, kMaximumSequentialPackets,
            net::CompletionCallback());
      }
      break;
    case 2:
      {
        NetworkStats* udp_stats_client = new NetworkStats();
        udp_stats_client->Start(
            host_resolver, server_address, histogram_port, has_proxy_server,
            kMediumTestBytesToSend, kMaximumSequentialPackets,
            net::CompletionCallback());
      }
      break;
    case 3:
      {
        NetworkStats* udp_stats_client = new NetworkStats();
        udp_stats_client->Start(
            host_resolver, server_address, histogram_port, has_proxy_server,
            kLargeTestBytesToSend, kMaximumSequentialPackets,
            net::CompletionCallback());
      }
      break;
  }
}

}  // namespace chrome_browser_net
