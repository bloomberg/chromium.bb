// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_stats.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/single_request_host_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/udp/datagram_client_socket.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chrome_browser_net {

// static
uint32 NetworkStats::maximum_tests_ = 8;
// static
uint32 NetworkStats::maximum_sequential_packets_ = 21;
// static
uint32 NetworkStats::maximum_NAT_packets_ = 2;
// static
uint32 NetworkStats::maximum_NAT_idle_seconds_ = 300;
// static
bool NetworkStats::start_test_after_connect_ = true;

// Specify the possible choices of probe packet sizes.
const uint32 kProbePacketBytes[] = {100, 500, 1200};
const uint32 kPacketSizeChoices = arraysize(kProbePacketBytes);

// List of ports used for probing test.
const uint16 kPorts[] = {443, 80};

// Number of first few packets that are recorded in a packet-correlation
// histogram, which shows exactly what sequence of packets were received.
// We use this to deduce specific packet loss correlation.
const uint32 kCorrelatedLossPacketCount = 6;

// This specifies the maximum message (payload) size of one packet.
const uint32 kMaxMessageSize = 1600;

// This specifies the maximum udp receiver buffer size.
const uint32 kMaxUdpReceiveBufferSize = 63000;

// This specifies the maximum udp receiver buffer size.
const uint32 kMaxUdpSendBufferSize = 4096;

// This should match TestType except for the last one.
const char* kTestName[] = {"TokenRequest", "StartPacket", "NonPacedPacket",
                           "PacedPacket", "NATBind", "PacketSizeTest"};

// Perform Pacing/Non-pacing test only if at least 2 packets are received
// in the StartPacketTest.
const uint32 kMinimumReceivedPacketsForPacingTest = 2;
// Perform NAT binding test only if at least 10 packets are received.
const uint32 kMinimumReceivedPacketsForNATTest = 10;

// Maximum inter-packet pacing interval in microseconds.
const uint32 kMaximumPacingMicros = 1000000;
// Timeout value for getting the token.
const uint32 kGetTokenTimeoutSeconds = 10;
// Timeout value for StartPacket and NonPacedPacket if the client does not get
// reply. For PacedPacket test, the timeout value is this number plus the total
// pacing interval.
const uint32 kReadDataTimeoutSeconds = 30;
// This is the timeout for NAT without Idle periods.
// For NAT test with idle periods, the timeout is the Idle period + this value.
const uint32 kReadNATTimeoutSeconds = 10;
// This is the timeout for PACKET_SIZE_TEST.
const uint32 kReadPacketSizeTimeoutSeconds = 10;
// This is the maxmium number of packets we would send for PACKET_SIZE_TEST.
uint32 kMaximumPacketSizeTestPackets = 1;

// These helper functions are similar to UMA_HISTOGRAM_XXX except that they do
// not create a static histogram_pointer.
void DynamicHistogramEnumeration(const std::string& name,
                                 uint32 sample,
                                 uint32 boundary_value) {
  base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
      name,
      1,
      boundary_value,
      boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(sample);
}

void DynamicHistogramTimes(const std::string& name,
                           const base::TimeDelta& sample) {
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryTimeGet(
      name,
      base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(30),
      50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->AddTime(sample);
}

void DynamicHistogramCounts(const std::string& name,
                            uint32 sample,
                            uint32 min,
                            uint32 max,
                            uint32 bucket_count) {
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryGet(
      name, min, max, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(sample);
}

NetworkStats::NetworkStats(net::ClientSocketFactory* socket_factory)
    : socket_factory_(socket_factory),
      histogram_port_(0),
      has_proxy_server_(false),
      probe_packet_bytes_(0),
      bytes_for_packet_size_test_(0),
      current_test_index_(0),
      read_state_(READ_STATE_IDLE),
      write_state_(WRITE_STATE_IDLE),
      weak_factory_(this) {
  ResetData();
}

NetworkStats::~NetworkStats() {}

bool NetworkStats::Start(net::HostResolver* host_resolver,
                         const net::HostPortPair& server_host_port_pair,
                         uint16 histogram_port,
                         bool has_proxy_server,
                         uint32 probe_bytes,
                         uint32 bytes_for_packet_size_test,
                         const net::CompletionCallback& finished_callback) {
  DCHECK(host_resolver);
  histogram_port_ = histogram_port;
  has_proxy_server_ = has_proxy_server;
  probe_packet_bytes_ = probe_bytes;
  bytes_for_packet_size_test_ = bytes_for_packet_size_test;
  finished_callback_ = finished_callback;
  test_sequence_.clear();
  test_sequence_.push_back(TOKEN_REQUEST);

  ResetData();

  scoped_ptr<net::SingleRequestHostResolver> resolver(
      new net::SingleRequestHostResolver(host_resolver));
  net::HostResolver::RequestInfo request(server_host_port_pair);
  int rv =
      resolver->Resolve(request,
                        net::DEFAULT_PRIORITY,
                        &addresses_,
                        base::Bind(base::IgnoreResult(&NetworkStats::DoConnect),
                                   base::Unretained(this)),
                        net::BoundNetLog());
  if (rv == net::ERR_IO_PENDING) {
    resolver_.swap(resolver);
    return true;
  }
  return DoConnect(rv);
}

void NetworkStats::StartOneTest() {
  if (test_sequence_[current_test_index_] == TOKEN_REQUEST) {
    DCHECK_EQ(WRITE_STATE_IDLE, write_state_);
    write_buffer_ = NULL;
    SendHelloRequest();
  } else {
    SendProbeRequest();
  }
}

void NetworkStats::ResetData() {
  DCHECK_EQ(WRITE_STATE_IDLE, write_state_);
  write_buffer_ = NULL;
  packets_received_mask_.reset();
  first_arrival_time_ = base::TimeTicks();
  last_arrival_time_ = base::TimeTicks();

  packet_rtt_.clear();
  packet_rtt_.resize(maximum_sequential_packets_);
  probe_request_time_ = base::TimeTicks();
  // Note: inter_arrival_time_ should not be reset here because it is used in
  // subsequent tests.
}

bool NetworkStats::DoConnect(int result) {
  if (result != net::OK) {
    TestPhaseComplete(RESOLVE_FAILED, result);
    return false;
  }

  scoped_ptr<net::DatagramClientSocket> udp_socket =
      socket_factory_->CreateDatagramClientSocket(
          net::DatagramSocket::DEFAULT_BIND,
          net::RandIntCallback(),
          NULL,
          net::NetLog::Source());
  DCHECK(udp_socket);
  DCHECK(!socket_);
  socket_ = udp_socket.Pass();

  const net::IPEndPoint& endpoint = addresses_.front();
  int rv = socket_->Connect(endpoint);
  if (rv < 0) {
    TestPhaseComplete(CONNECT_FAILED, rv);
    return false;
  }

  socket_->SetSendBufferSize(kMaxUdpSendBufferSize);
  socket_->SetReceiveBufferSize(kMaxUdpReceiveBufferSize);
  return ConnectComplete(rv);
}

bool NetworkStats::ConnectComplete(int result) {
  if (result < 0) {
    TestPhaseComplete(CONNECT_FAILED, result);
    return false;
  }

  if (start_test_after_connect_) {
    // Reads data for all HelloReply and all subsequent probe tests.
    if (ReadData() != net::ERR_IO_PENDING) {
      TestPhaseComplete(READ_FAILED, result);
      return false;
    }
    SendHelloRequest();
  } else {
    // For unittesting. Only run the callback, do not destroy it.
    if (!finished_callback_.is_null())
      finished_callback_.Run(result);
  }
  return true;
}

void NetworkStats::SendHelloRequest() {
  StartReadDataTimer(kGetTokenTimeoutSeconds, current_test_index_);
  ProbePacket probe_packet;
  probe_message_.SetPacketHeader(ProbePacket_Type_HELLO_REQUEST, &probe_packet);
  probe_packet.set_group_id(current_test_index_);
  std::string output = probe_message_.MakeEncodedPacket(probe_packet);

  int result = SendData(output);
  if (result < 0 && result != net::ERR_IO_PENDING)
    TestPhaseComplete(WRITE_FAILED, result);
}

void NetworkStats::SendProbeRequest() {
  ResetData();
  // Use default timeout except for the NAT bind test.
  uint32 timeout_seconds = kReadDataTimeoutSeconds;
  uint32 number_packets = maximum_sequential_packets_;
  uint32 probe_bytes = probe_packet_bytes_;
  pacing_interval_ = base::TimeDelta();
  switch (test_sequence_[current_test_index_]) {
    case START_PACKET_TEST:
    case NON_PACED_PACKET_TEST:
      break;
    case PACED_PACKET_TEST: {
      pacing_interval_ =
          std::min(inter_arrival_time_,
                   base::TimeDelta::FromMicroseconds(kMaximumPacingMicros));
      timeout_seconds += pacing_interval_.InMicroseconds() *
                         (maximum_sequential_packets_ - 1) / 1000000;
      break;
    }
    case NAT_BIND_TEST: {
      // Make sure no integer overflow.
      DCHECK_LE(maximum_NAT_idle_seconds_, 4000U);
      int nat_test_idle_seconds = base::RandInt(1, maximum_NAT_idle_seconds_);
      pacing_interval_ = base::TimeDelta::FromSeconds(nat_test_idle_seconds);
      timeout_seconds = nat_test_idle_seconds + kReadNATTimeoutSeconds;
      number_packets = maximum_NAT_packets_;
      break;
    }
    case PACKET_SIZE_TEST: {
      number_packets = kMaximumPacketSizeTestPackets;
      probe_bytes = bytes_for_packet_size_test_;
      timeout_seconds = kReadPacketSizeTimeoutSeconds;
      break;
    }
    default:
      NOTREACHED();
      return;
  }
  DVLOG(1) << "NetworkStat: Probe pacing " << pacing_interval_.InMicroseconds()
           << " microseconds. Time out " << timeout_seconds << " seconds";
  ProbePacket probe_packet;
  probe_message_.GenerateProbeRequest(token_,
                                      current_test_index_,
                                      probe_bytes,
                                      pacing_interval_.InMicroseconds(),
                                      number_packets,
                                      &probe_packet);
  std::string output = probe_message_.MakeEncodedPacket(probe_packet);

  StartReadDataTimer(timeout_seconds, current_test_index_);
  probe_request_time_ = base::TimeTicks::Now();
  int result = SendData(output);
  if (result < 0 && result != net::ERR_IO_PENDING)
    TestPhaseComplete(WRITE_FAILED, result);
}

int NetworkStats::ReadData() {
  if (!socket_.get())
    return 0;

  if (read_state_ == READ_STATE_READ_PENDING)
    return net::ERR_IO_PENDING;

  int rv = 0;
  while (true) {
    DCHECK(!read_buffer_.get());
    read_buffer_ = new net::IOBuffer(kMaxMessageSize);

    rv = socket_->Read(
        read_buffer_.get(),
        kMaxMessageSize,
        base::Bind(&NetworkStats::OnReadComplete, weak_factory_.GetWeakPtr()));
    if (rv <= 0)
      break;
    if (ReadComplete(rv))
      return rv;
  }
  if (rv == net::ERR_IO_PENDING)
    read_state_ = READ_STATE_READ_PENDING;
  return rv;
}

void NetworkStats::OnReadComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DCHECK_EQ(READ_STATE_READ_PENDING, read_state_);

  read_state_ = READ_STATE_IDLE;
  if (!ReadComplete(result)) {
    // Called ReadData() via PostDelayedTask() to avoid recursion. Added a delay
    // of 1ms so that the time-out will fire before we have time to really hog
    // the CPU too extensively (waiting for the time-out) in case of an infinite
    // loop.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&NetworkStats::ReadData),
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(1));
  }
}

bool NetworkStats::ReadComplete(int result) {
  DCHECK(socket_.get());
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result < 0) {
    // Something is wrong, finish the test.
    read_buffer_ = NULL;
    TestPhaseComplete(READ_FAILED, result);
    return true;
  }

  std::string encoded_message(read_buffer_->data(),
                              read_buffer_->data() + result);
  read_buffer_ = NULL;
  ProbePacket probe_packet;
  if (!probe_message_.ParseInput(encoded_message, &probe_packet))
    return false;
  // Discard if the packet is for a different test.
  if (probe_packet.group_id() != current_test_index_)
    return false;

  // Whether all packets in the current test have been received.
  bool current_test_complete = false;
  switch (probe_packet.header().type()) {
    case ProbePacket_Type_HELLO_REPLY:
      token_ = probe_packet.token();
      if (current_test_index_ == 0)
        test_sequence_.push_back(START_PACKET_TEST);
      current_test_complete = true;
      break;
    case ProbePacket_Type_PROBE_REPLY:
      current_test_complete = UpdateReception(probe_packet);
      break;
    default:
      DVLOG(1) << "Received unexpected packet type: "
               << probe_packet.header().type();
  }

  if (!current_test_complete) {
    // All packets have not been received for the current test.
    return false;
  }
  // All packets are received for the current test.
  // Read completes if all tests are done (if TestPhaseComplete didn't start
  // another test).
  return TestPhaseComplete(SUCCESS, net::OK);
}

bool NetworkStats::UpdateReception(const ProbePacket& probe_packet) {
  uint32 packet_index = probe_packet.packet_index();
  if (packet_index >= packet_rtt_.size())
    return false;
  packets_received_mask_.set(packet_index);
  TestType test_type = test_sequence_[current_test_index_];
  uint32 received_packets = packets_received_mask_.count();

  // Now() has resolution ~1-15ms. HighResNow() has high resolution but it
  // is warned not to use it unless necessary.
  base::TimeTicks current_time = base::TimeTicks::Now();
  last_arrival_time_ = current_time;
  if (first_arrival_time_.is_null())
    first_arrival_time_ = current_time;

  // Need to do this after updating the last_arrival_time_ since NAT_BIND_TEST
  // and PACKET_SIZE_TEST record the SendToLastRecvDelay.
  if (test_type == NAT_BIND_TEST) {
    return received_packets >= maximum_NAT_packets_;
  }
  if (test_type == PACKET_SIZE_TEST) {
    return received_packets >= kMaximumPacketSizeTestPackets;
  }

  base::TimeDelta rtt =
      current_time - probe_request_time_ -
      base::TimeDelta::FromMicroseconds(std::max(
          static_cast<int64>(0), probe_packet.server_processing_micros()));
  base::TimeDelta min_rtt = base::TimeDelta::FromMicroseconds(1L);
  packet_rtt_[packet_index] = (rtt >= min_rtt) ? rtt : min_rtt;

  if (received_packets < maximum_sequential_packets_)
    return false;
  // All packets in the current test are received.
  inter_arrival_time_ = (last_arrival_time_ - first_arrival_time_) /
      std::max(1U, (received_packets - 1));
  if (test_type == START_PACKET_TEST) {
    test_sequence_.push_back(PACKET_SIZE_TEST);
    test_sequence_.push_back(TOKEN_REQUEST);
    // No need to add TOKEN_REQUEST here when all packets are received.
    test_sequence_.push_back(base::RandInt(0, 1) ? PACED_PACKET_TEST
                                                 : NON_PACED_PACKET_TEST);
    test_sequence_.push_back(TOKEN_REQUEST);
    test_sequence_.push_back(NAT_BIND_TEST);
    test_sequence_.push_back(TOKEN_REQUEST);
  }
  return true;
}

int NetworkStats::SendData(const std::string& output) {
  if (write_buffer_.get() || !socket_.get() ||
      write_state_ == WRITE_STATE_WRITE_PENDING) {
    return net::ERR_UNEXPECTED;
  }
  scoped_refptr<net::StringIOBuffer> buffer(new net::StringIOBuffer(output));
  write_buffer_ = new net::DrainableIOBuffer(buffer.get(), buffer->size());

  int bytes_written = socket_->Write(
      write_buffer_.get(),
      write_buffer_->BytesRemaining(),
      base::Bind(&NetworkStats::OnWriteComplete, weak_factory_.GetWeakPtr()));
  if (bytes_written < 0) {
    if (bytes_written == net::ERR_IO_PENDING)
      write_state_ = WRITE_STATE_WRITE_PENDING;
    return bytes_written;
  }
  UpdateSendBuffer(bytes_written);
  return net::OK;
}

void NetworkStats::OnWriteComplete(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DCHECK_EQ(WRITE_STATE_WRITE_PENDING, write_state_);
  write_state_ = WRITE_STATE_IDLE;
  if (result < 0 || !socket_.get() || write_buffer_.get() == NULL) {
    TestPhaseComplete(WRITE_FAILED, result);
    return;
  }
  UpdateSendBuffer(result);
}

void NetworkStats::UpdateSendBuffer(int bytes_sent) {
  write_buffer_->DidConsume(bytes_sent);
  DCHECK_EQ(write_buffer_->BytesRemaining(), 0);
  DCHECK_EQ(WRITE_STATE_IDLE, write_state_);
  write_buffer_ = NULL;
}

void NetworkStats::StartReadDataTimer(uint32 seconds, uint32 test_index) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NetworkStats::OnReadDataTimeout,
                 weak_factory_.GetWeakPtr(),
                 test_index),
      base::TimeDelta::FromSeconds(seconds));
}

void NetworkStats::OnReadDataTimeout(uint32 test_index) {
  // If the current_test_index_ has changed since we set the timeout,
  // the current test has been completed, so do nothing.
  if (test_index != current_test_index_)
    return;
  // If test_type is TOKEN_REQUEST, it will do nothing but call
  // TestPhaseComplete().
  TestType test_type = test_sequence_[current_test_index_];

  uint32 received_packets = packets_received_mask_.count();
  if (received_packets >= 2) {
    inter_arrival_time_ =
        (last_arrival_time_ - first_arrival_time_) / (received_packets - 1);
  }
  // Add other tests if this is START_PACKET_TEST.
  if (test_type == START_PACKET_TEST) {
    if (received_packets >= kMinimumReceivedPacketsForPacingTest) {
      test_sequence_.push_back(TOKEN_REQUEST);
      test_sequence_.push_back(PACKET_SIZE_TEST);
      test_sequence_.push_back(TOKEN_REQUEST);
      test_sequence_.push_back(base::RandInt(0, 1) ? PACED_PACKET_TEST
                                                   : NON_PACED_PACKET_TEST);
    }
    if (received_packets >= kMinimumReceivedPacketsForNATTest) {
      test_sequence_.push_back(TOKEN_REQUEST);
      test_sequence_.push_back(NAT_BIND_TEST);
      test_sequence_.push_back(TOKEN_REQUEST);
    }
  }
  TestPhaseComplete(READ_TIMED_OUT, net::ERR_FAILED);
}

bool NetworkStats::TestPhaseComplete(Status status, int result) {
  // If there is no valid token, do nothing and delete self.
  // This includes all connection error, name resolve error, etc.
  if (write_state_ == WRITE_STATE_WRITE_PENDING) {
    UMA_HISTOGRAM_BOOLEAN("NetConnectivity5.TestFailed.WritePending", true);
  } else if (status == SUCCESS || status == READ_TIMED_OUT) {
    TestType current_test = test_sequence_[current_test_index_];
    DCHECK_LT(current_test, TEST_TYPE_MAX);
    if (current_test != TOKEN_REQUEST) {
      RecordHistograms(current_test);
    } else if (current_test_index_ > 0) {
      if (test_sequence_[current_test_index_ - 1] == NAT_BIND_TEST) {
        // We record the NATTestReceivedHistograms after the succeeding
        // TokenRequest.
        RecordNATTestReceivedHistograms(status);
      } else if (test_sequence_[current_test_index_ - 1] == PACKET_SIZE_TEST) {
        // We record the PacketSizeTestReceivedHistograms after the succeeding
        // TokenRequest.
        RecordPacketSizeTestReceivedHistograms(status);
      }
    }

    // Move to the next test.
    current_test = GetNextTest();
    if (current_test_index_ <= maximum_tests_ && current_test < TEST_TYPE_MAX) {
      DVLOG(1) << "NetworkStat: Start Probe test: " << current_test;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&NetworkStats::StartOneTest, weak_factory_.GetWeakPtr()));
      return false;
    }
  }

  // All tests are done.
  DoFinishCallback(result);

  // Close the socket so that there are no more IO operations.
  if (socket_.get())
    socket_->Close();

  DVLOG(1) << "NetworkStat: schedule delete self at test index "
           << current_test_index_;
  delete this;
  return true;
}

NetworkStats::TestType NetworkStats::GetNextTest() {
  ++current_test_index_;
  if (current_test_index_ >= test_sequence_.size())
    return TEST_TYPE_MAX;
  return test_sequence_[current_test_index_];
}

void NetworkStats::DoFinishCallback(int result) {
  if (!finished_callback_.is_null()) {
    net::CompletionCallback callback = finished_callback_;
    finished_callback_.Reset();
    callback.Run(result);
  }
}

void NetworkStats::RecordHistograms(TestType test_type) {
  switch (test_type) {
    case START_PACKET_TEST:
    case NON_PACED_PACKET_TEST:
    case PACED_PACKET_TEST: {
      RecordInterArrivalHistograms(test_type);
      RecordPacketLossSeriesHistograms(test_type);
      RecordPacketsReceivedHistograms(test_type);
      // Only record RTT for these packet indices.
      uint32 rtt_indices[] = {0, 1, 2, 9, 19};
      for (uint32 i = 0; i < arraysize(rtt_indices); ++i) {
        if (rtt_indices[i] < packet_rtt_.size())
          RecordRTTHistograms(test_type, rtt_indices[i]);
      }
      RecordSendToLastRecvDelayHistograms(test_type);
      return;
    }
    case NAT_BIND_TEST:
      RecordSendToLastRecvDelayHistograms(test_type);
      return;
    case PACKET_SIZE_TEST:
      // No need to record RTT for PacketSizeTest.
      return;
    default:
      DVLOG(1) << "Unexpected test type " << test_type
               << " in RecordHistograms.";
  }
}

void NetworkStats::RecordInterArrivalHistograms(TestType test_type) {
  DCHECK_NE(test_type, PACKET_SIZE_TEST);
  std::string histogram_name =
      base::StringPrintf("NetConnectivity5.%s.Sent%d.PacketDelay.%d.%dB",
                         kTestName[test_type],
                         maximum_sequential_packets_,
                         histogram_port_,
                         probe_packet_bytes_);
  // Record the time normalized to 20 packet inter-arrivals.
  DynamicHistogramTimes(histogram_name, inter_arrival_time_ * 20);
}

void NetworkStats::RecordPacketsReceivedHistograms(TestType test_type) {
  DCHECK_NE(test_type, PACKET_SIZE_TEST);
  const char* test_name = kTestName[test_type];
  std::string histogram_prefix = base::StringPrintf(
      "NetConnectivity5.%s.Sent%d.", test_name, maximum_sequential_packets_);
  std::string histogram_suffix =
      base::StringPrintf(".%d.%dB", histogram_port_, probe_packet_bytes_);
  std::string name = histogram_prefix + "GotAPacket" + histogram_suffix;
  base::HistogramBase* histogram_pointer = base::BooleanHistogram::FactoryGet(
      name, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(packets_received_mask_.any());

  DynamicHistogramEnumeration(
      histogram_prefix + "PacketsRecv" + histogram_suffix,
      packets_received_mask_.count(),
      maximum_sequential_packets_ + 1);

  if (!packets_received_mask_.any())
    return;

  base::HistogramBase* received_nth_packet_histogram =
      base::Histogram::FactoryGet(
          histogram_prefix + "RecvNthPacket" + histogram_suffix,
          1,
          maximum_sequential_packets_ + 1,
          maximum_sequential_packets_ + 2,
          base::HistogramBase::kUmaTargetedHistogramFlag);

  int count = 0;
  for (size_t j = 0; j < maximum_sequential_packets_; ++j) {
    int packet_number = j + 1;
    if (packets_received_mask_.test(j)) {
      received_nth_packet_histogram->Add(packet_number);
      ++count;
    }
    std::string histogram_name =
        base::StringPrintf("%sNumRecvFromFirst%02dPackets%s",
                           histogram_prefix.c_str(),
                           packet_number,
                           histogram_suffix.c_str());
    DynamicHistogramEnumeration(histogram_name, count, packet_number + 1);
  }
}

void NetworkStats::RecordNATTestReceivedHistograms(Status status) {
  const char* test_name = kTestName[NAT_BIND_TEST];
  bool test_result = status == SUCCESS;
  std::string middle_name = test_result ? "Connectivity.Success"
                                        : "Connectivity.Failure";
  // Record whether the HelloRequest got reply successfully.
  std::string histogram_name =
      base::StringPrintf("NetConnectivity5.%s.Sent%d.%s.%d.%dB",
                         test_name,
                         maximum_NAT_packets_,
                         middle_name.c_str(),
                         histogram_port_,
                         probe_packet_bytes_);
  uint32 bucket_count = std::min(maximum_NAT_idle_seconds_ + 2, 50U);
  DynamicHistogramCounts(histogram_name,
                         pacing_interval_.InSeconds(),
                         1,
                         maximum_NAT_idle_seconds_ + 1,
                         bucket_count);

  // Record the NAT bind result only if the HelloRequest successfully got the
  // token and the first NAT test packet is received.
  if (!test_result || !packets_received_mask_.test(0))
    return;

  middle_name = packets_received_mask_.test(1) ? "Bind.Success"
                                               : "Bind.Failure";
  histogram_name = base::StringPrintf("NetConnectivity5.%s.Sent%d.%s.%d.%dB",
                                      test_name,
                                      maximum_NAT_packets_,
                                      middle_name.c_str(),
                                      histogram_port_,
                                      probe_packet_bytes_);
  DynamicHistogramCounts(histogram_name,
                         pacing_interval_.InSeconds(),
                         1,
                         maximum_NAT_idle_seconds_ + 1,
                         bucket_count);
}

void NetworkStats::RecordPacketSizeTestReceivedHistograms(Status status) {
  const char* test_name = kTestName[PACKET_SIZE_TEST];
  bool test_result = (status == SUCCESS && packets_received_mask_.test(0));
  std::string middle_name = test_result ? "Connectivity.Success"
                                        : "Connectivity.Failure";
  // Record whether the HelloRequest got reply successfully.
  std::string histogram_name =
      base::StringPrintf("NetConnectivity5.%s.%s.%d",
                         test_name,
                         middle_name.c_str(),
                         histogram_port_);
  base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
      histogram_name, kProbePacketBytes[kPacketSizeChoices - 1],
      ProbeMessage::kMaxProbePacketBytes, 60,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(bytes_for_packet_size_test_);
}

void NetworkStats::RecordPacketLossSeriesHistograms(TestType test_type) {
  DCHECK_NE(test_type, PACKET_SIZE_TEST);
  const char* test_name = kTestName[test_type];
  // Build "NetConnectivity5.<TestName>.First6.SeriesRecv.<port>.<probe_size>"
  // histogram name. Total 3(tests) x 12 histograms.
  std::string series_acked_histogram_name =
      base::StringPrintf("NetConnectivity5.%s.First6.SeriesRecv.%d.%dB",
                         test_name,
                         histogram_port_,
                         probe_packet_bytes_);
  uint32 histogram_boundary = 1 << kCorrelatedLossPacketCount;
  uint32 correlated_packet_mask =
      (histogram_boundary - 1) & packets_received_mask_.to_ulong();
  DynamicHistogramEnumeration(
      series_acked_histogram_name, correlated_packet_mask, histogram_boundary);

  // If we are running without a proxy, we'll generate an extra histogram with
  // the ".NoProxy" suffix.
  if (!has_proxy_server_) {
    series_acked_histogram_name.append(".NoProxy");
    DynamicHistogramEnumeration(series_acked_histogram_name,
                                correlated_packet_mask,
                                histogram_boundary);
  }
}

void NetworkStats::RecordRTTHistograms(TestType test_type, uint32 index) {
  DCHECK_NE(test_type, PACKET_SIZE_TEST);
  DCHECK_LT(index, packet_rtt_.size());

  if (!packets_received_mask_.test(index))
    return;  // Probe packet never received.

  std::string rtt_histogram_name = base::StringPrintf(
      "NetConnectivity5.%s.Sent%d.Success.RTT.Packet%02d.%d.%dB",
      kTestName[test_type],
      maximum_sequential_packets_,
      index + 1,
      histogram_port_,
      probe_packet_bytes_);
  DynamicHistogramTimes(rtt_histogram_name, packet_rtt_[index]);
}

void NetworkStats::RecordSendToLastRecvDelayHistograms(TestType test_type) {
  DCHECK_NE(test_type, PACKET_SIZE_TEST);
  if (packets_received_mask_.count() < 2)
    return;  // Too few packets are received.
  uint32 packets_sent = test_type == NAT_BIND_TEST
      ? maximum_NAT_packets_ : maximum_sequential_packets_;
  std::string histogram_name = base::StringPrintf(
      "NetConnectivity5.%s.Sent%d.SendToLastRecvDelay.%d.%dB",
      kTestName[test_type],
      packets_sent,
      histogram_port_,
      probe_packet_bytes_);
  base::TimeDelta send_to_last_recv_time =
      std::max(last_arrival_time_ - probe_request_time_ -
                   pacing_interval_ * (packets_sent - 1),
               base::TimeDelta::FromMilliseconds(0));
  DynamicHistogramTimes(histogram_name, send_to_last_recv_time);
}

// ProxyDetector methods and members.
ProxyDetector::ProxyDetector(net::ProxyService* proxy_service,
                             const net::HostPortPair& server_address,
                             OnResolvedCallback callback)
    : proxy_service_(proxy_service),
      server_address_(server_address),
      callback_(callback),
      has_pending_proxy_resolution_(false) {}

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
      net::LOAD_NORMAL,
      &proxy_info_,
      base::Bind(&ProxyDetector::OnResolveProxyComplete,
                 base::Unretained(this)),
      NULL,
      NULL,
      net::BoundNetLog());
  if (rv != net::ERR_IO_PENDING)
    OnResolveProxyComplete(rv);
}

void ProxyDetector::OnResolveProxyComplete(int result) {
  has_pending_proxy_resolution_ = false;
  bool has_proxy_server =
      (result == net::OK && proxy_info_.proxy_server().is_valid() &&
       !proxy_info_.proxy_server().is_direct());

  OnResolvedCallback callback = callback_;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, has_proxy_server));

  // TODO(rtenneti): Will we leak if ProxyResolve is cancelled (or proxy
  // resolution never completes).
  delete this;
}

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
        base::Bind(&CollectNetworkStats, network_stats_server, io_thread));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (net::NetworkChangeNotifier::IsOffline()) {
    return;
  }

  CR_DEFINE_STATIC_LOCAL(scoped_refptr<base::FieldTrial>, trial, ());
  static bool collect_stats = false;

  if (!trial.get()) {
    // Set up a field trial to collect network stats for UDP.
    const base::FieldTrial::Probability kDivisor = 1000;

    // Enable the connectivity testing for 0.5% of the users in stable channel.
    base::FieldTrial::Probability probability_per_group = kDivisor / 200;

    chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
    if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
      // Enable the connectivity testing for 50% of the users in canary channel.
      probability_per_group = kDivisor / 2;
    } else if (channel == chrome::VersionInfo::CHANNEL_DEV) {
      // Enable the connectivity testing for 10% of the users in dev channel.
      probability_per_group = kDivisor / 10;
    } else if (channel == chrome::VersionInfo::CHANNEL_BETA) {
      // Enable the connectivity testing for 1% of the users in beta channel.
      probability_per_group = kDivisor / 100;
    }

    // After July 31, 2014 builds, it will always be in default group
    // (disable_network_stats).
    trial = base::FieldTrialList::FactoryGetFieldTrial(
        "NetworkConnectivity", kDivisor, "disable_network_stats",
        2014, 7, 31, base::FieldTrial::SESSION_RANDOMIZED, NULL);

    // Add option to collect_stats for NetworkConnectivity.
    int collect_stats_group =
        trial->AppendGroup("collect_stats", probability_per_group);
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

  uint32 port_index = base::RandInt(0, arraysize(kPorts) - 1);
  uint16 histogram_port = kPorts[port_index];
  net::HostPortPair server_address(network_stats_server, histogram_port);

  net::ProxyService* proxy_service =
      io_thread->globals()->system_proxy_service.get();
  DCHECK(proxy_service);

  ProxyDetector::OnResolvedCallback callback = base::Bind(
      &StartNetworkStatsTest, host_resolver, server_address, histogram_port);

  ProxyDetector* proxy_client =
      new ProxyDetector(proxy_service, server_address, callback);
  proxy_client->StartResolveProxy();
}

void StartNetworkStatsTest(net::HostResolver* host_resolver,
                           const net::HostPortPair& server_address,
                           uint16 histogram_port,
                           bool has_proxy_server) {
  int probe_choice = base::RandInt(0, kPacketSizeChoices - 1);

  DCHECK_LE(ProbeMessage::kMaxProbePacketBytes, kMaxMessageSize);
  // Pick a packet size between 1200 and kMaxProbePacketBytes bytes.
  uint32 bytes_for_packet_size_test =
      base::RandInt(kProbePacketBytes[kPacketSizeChoices - 1],
                    ProbeMessage::kMaxProbePacketBytes);

  // |udp_stats_client| is owned and deleted in the class NetworkStats.
  NetworkStats* udp_stats_client =
      new NetworkStats(net::ClientSocketFactory::GetDefaultFactory());
  udp_stats_client->Start(host_resolver,
                          server_address,
                          histogram_port,
                          has_proxy_server,
                          kProbePacketBytes[probe_choice],
                          bytes_for_packet_size_test,
                          net::CompletionCallback());
}

}  // namespace chrome_browser_net
