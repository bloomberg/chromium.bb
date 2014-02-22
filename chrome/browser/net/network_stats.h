// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NETWORK_STATS_H_
#define CHROME_BROWSER_NET_NETWORK_STATS_H_

#include <bitset>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/probe_message.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/proxy/proxy_info.h"

namespace net {
class ClientSocketFactory;
class DatagramClientSocket;
class HostResolver;
class SingleRequestHostResolver;
}

namespace chrome_browser_net {

// This class is used for live experiment of network connectivity (for UDP)
// metrics. A small percentage of users participate in this experiment. All
// users (who are in the experiment) must have enabled "UMA upload".
// In the experiments, clients request the server to send some probing packets,
// collect some stats, and send back the results via UMA reports.
//
// This class collects the following stats from users who have opted in.
// a) RTT.
// b) packet inter-arrival time.
// c) packet losses for correlation and FEC experiments.
// d) packet losses for NAT binding test after idling for a certain period.
//
// There are three tests in one experiment. Right before each test, a
// HelloRequest is sent to get an updated token.
// 1. |START_PACKET_TEST|: 21 packets are sent from the server to the client
//    without pacing.
// 2. |PACED_PACKET_TEST| or |NON_PACED_PACKET_TEST|: After the first test,
//    21 packets are sent from the server to the client with or without pacing.
//    If pacing, the pacing rate is computed from the first test.
// 3. |NAT_BIND_TEST|: 2 packets are sent from the server to the client with
//    a randomly generated delay of 1~300 seconds.
// At the end of these tests, we send another HelloRequest to test whether
// the network is still connected and has not changed (e.g. from Wifi to 3g).

class NetworkStats {
 public:
  enum Status {            // Used in HISTOGRAM_ENUMERATION.
    SUCCESS,               // Successfully received bytes from the server.
    SOCKET_CREATE_FAILED,  // Socket creation failed.
    RESOLVE_FAILED,        // Host resolution failed.
    CONNECT_FAILED,        // Connection to the server failed.
    WRITE_FAILED,          // Sending a message to the server failed.
    READ_TIMED_OUT,        // Reading the reply from the server timed out.
    READ_FAILED,           // Reading the reply from the server failed.
    STATUS_MAX,            // Bounding value.
  };

  enum ReadState {         // Used to track if |socket_| has a pending read.
    READ_STATE_IDLE,
    READ_STATE_READ_PENDING,
  };

  enum WriteState {        // Used to track if |socket_| has a pending write.
    WRITE_STATE_IDLE,
    WRITE_STATE_WRITE_PENDING,
  };

  // |TestType| specifies the possible tests we may run
  // (except for the first and the last serving as boundaries).
  enum TestType {
    // The first one is for requesting token, not a probe test. Put it here
    // because we use it as a symbol for sending the HelloRequest packet to
    // acquire the token.
    TOKEN_REQUEST,
    START_PACKET_TEST,      // First packet loss test (no pacing).
    NON_PACED_PACKET_TEST,  // Packet loss test with no pacing.
    PACED_PACKET_TEST,      // Packet loss test with pacing.
    // Test whether NAT binding expires after some idle period.
    NAT_BIND_TEST,
    PACKET_SIZE_TEST,
    TEST_TYPE_MAX,
  };

  // Pointer |socket_factory| is NOT deleted by this class.
  explicit NetworkStats(net::ClientSocketFactory* socket_factory);
  // NetworkStats is deleted in TestPhaseComplete() when all tests are done.
  ~NetworkStats();

  // Start the client and connect to |server|.
  // A client will request a token and then perform several tests.
  // When the client finishes all tests, or when an error occurs causing the
  // client to stop, |TestPhaseComplete| will be called with a net status code.
  // |TestPhaseComplete| will collect histogram stats.
  // Return true if successful in starting the client.
  bool Start(net::HostResolver* host_resolver,
             const net::HostPortPair& server,
             uint16 histogram_port,
             bool has_proxy_server,
             uint32 probe_bytes,
             uint32 bytes_for_packet_size_test,
             const net::CompletionCallback& callback);

 private:
  friend class NetworkStatsTest;

  // Start the test specified by the current_test_index_. It also resets all
  // the book keeping data, before starting the new test.
  void StartOneTest();

  // Reset all the counters and the collected stats.
  void ResetData();

  // Callback that is called when host resolution is completed.
  void OnResolveComplete(int result);

  // Called after host is resolved. Creates UDPClientSocket and connects to the
  // server. If successfully connected, then calls ConnectComplete() to start
  // the network connectivity tests. Returns |false| if there is any error.
  bool DoConnect(int result);

  // This method is called after socket connection is completed. It will start
  // the process of sending packets to |server| by calling SendHelloPacket().
  // Return false if connection is not established (result is less than 0).
  bool ConnectComplete(int result);

  // Send a HelloRequest packet which asks for a token from the server. If
  // a token is received, it will will add |START_PACKET_TEST| to the test
  // queue.
  void SendHelloRequest();

  // Send a ProbeRequest packet which requests the server to send a set
  // of Probing packets.
  void SendProbeRequest();

  // Read and process the data. Called from OnReadComplete() or ReadData().
  // This function calls TestPhaseComplete() if there is a significant network
  // error or if all packets in the current test are received.
  // Return true if TestPhaseComplete() is called otherwise return false.
  bool ReadComplete(int result);

  // Callbacks when an internal IO (Read or Write) is completed.
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  // Read data from server until an error or IO blocking occurs or reading is
  // complete. Return the result value from socket reading and 0 if |socket_|
  // is Null.
  int ReadData();

  // Send data contained in |str| to server.
  // Return a negative value if IO blocking occurs or there is an error.
  // Otherwise return net::OK.
  int SendData(const std::string& str);

  // Update the send buffer (telling it that |bytes_sent| has been sent).
  // And reset |write_buffer_|.
  void UpdateSendBuffer(int bytes_sent);

  // Start a timer (with value |milliseconds|) for responses from the probe
  // servers.  |test_index| is the index of the test at vector |test_sequence_|
  // and it is used as a parameter of the timer callback.
  void StartReadDataTimer(uint32 milliseconds, uint32 test_index);

  // Called when the StartReadDataTimer fires. |test_index| specifies
  // the index of the test. If |current_test_index_| has changed to a
  // different value, it indicates |test_index| has completed, then
  // this method is a no-op.
  void OnReadDataTimeout(uint32 test_index);

  // Collect network connectivity stats. This is called when all the data from
  // server is read or when there is a failure during connect/read/write. It
  // will either start the next phase of the test, or it will self destruct
  // at the end of this method. Returns true if a new test wasn't started and it
  // was self destructed.
  bool TestPhaseComplete(Status status, int result);

  // This method is called from TestPhaseComplete() and calls
  // |finished_callback_| callback to indicate that the test has finished.
  void DoFinishCallback(int result);

  // Update counters/metrics for the given |probe_packet|.
  // Return true if all packets for the current test are received and
  // false otherwise.
  bool UpdateReception(const ProbePacket& probe_packet);

  // Record all histograms for current test phase, which is assumed to be
  // complete (i.e., we are no longer waiting for packets in this phase).
  // |test_type| is the current test_type to be recorded. |status| is the
  // status of the current test.
  void RecordHistograms(TestType test_type);

  // Collect the following network connectivity stats when
  // kMaximumSequentialPackets (21) packets are sent from the server.
  // a) Client received at least one packet.
  // b) Client received the nth packet.
  // c) The number of packets received for each subsequence of packets 1...n.
  void RecordPacketsReceivedHistograms(TestType test_type);

  // Collect the following network connectivity stats for the first
  // kMaximumCorrelationPackets (6) packets in a test.
  // Success/failure of each packet, to estimate reachability for users,
  // and to estimate if there is a probabalistic dependency in packet loss when
  // kMaximumCorrelationPackets packets are sent consecutively.
  void RecordPacketLossSeriesHistograms(TestType test_type);

  // Collect the average inter-arrival time (scaled up by 20 times because the
  // minimum time value in a histogram is 1ms) of a sequence of probing packets.
  void RecordInterArrivalHistograms(TestType test_type);

  // Collect the RTT for the packet specified by the |index| in the current
  // test.
  void RecordRTTHistograms(TestType test_type, uint32 index);

  // Collect whether the second packet in the NAT test is received for the
  // given idle time.
  void RecordNATTestReceivedHistograms(Status status);

  // Collect whether we have the requested packet size was received or not in
  // the PACKET_SIZE_TEST test.
  void RecordPacketSizeTestReceivedHistograms(Status status);

  // Record the time duration between sending the probe request and receiving
  // the last probe packet excluding the pacing time requested by the client.
  // This applies to both NAT bind test and paced/non-paced packet test.
  void RecordSendToLastRecvDelayHistograms(TestType test_type);

  // Return the next test type (internally increment |current_test_index_|)
  // in |test_sequence_|;
  TestType GetNextTest();

  // These static variables are defined so that they can be changed in testing.
  // Maximum number of tests in one activation of the experiment.
  static uint32 maximum_tests_;
  // Maximum number of packets for START/PACED/NON_PACED tests.
  static uint32 maximum_sequential_packets_;
  // Maximum number of packets for NAT binding test.
  static uint32 maximum_NAT_packets_;
  // Maximum time duration between the two packets for NAT Bind testing.
  static uint32 maximum_NAT_idle_seconds_;
  // Whether to start the probe test immediately after connect success.
  // Used for unittest.
  static bool start_test_after_connect_;

  // The socket handler for this session.
  scoped_ptr<net::DatagramClientSocket> socket_;

  net::ClientSocketFactory* socket_factory_;

  // The read buffer used to read data from the socket.
  scoped_refptr<net::IOBuffer> read_buffer_;

  // The write buffer used to write data to the socket.
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  // Specify the port for which we are testing the network connectivity.
  uint16 histogram_port_;

  // Specify if there is a proxy server or not.
  bool has_proxy_server_;

  // HostResolver used to find the IP addresses.
  scoped_ptr<net::SingleRequestHostResolver> resolver_;

  // Addresses filled out by HostResolver after host resolution is completed.
  net::AddressList addresses_;

  // Callback to call when test is successefully finished or whenever
  // there is an error (this will be used by unittests to check the result).
  net::CompletionCallback finished_callback_;

  // RTTs for each packet.
  std::vector<base::TimeDelta> packet_rtt_;

  // Time when sending probe_request, used for computing RTT.
  base::TimeTicks probe_request_time_;

  // Size of the probe packets requested to be sent from servers. We don't use
  // |probe_packet_bytes_| during PACKET_SIZE_TEST.
  uint32 probe_packet_bytes_;

  // Size of the packet requested to be sent from servers for PACKET_SIZE_TEST.
  uint32 bytes_for_packet_size_test_;

  // bitmask indicating which packets are received.
  std::bitset<21> packets_received_mask_;

  // Arrival time of the first packet in the current test.
  base::TimeTicks first_arrival_time_;
  // Arrival time of the most recently received packet in the current test.
  base::TimeTicks last_arrival_time_;
  // Average time between two consecutive packets. It is updated when either all
  // packets are received or timeout happens in the current test.
  base::TimeDelta inter_arrival_time_;
  // Target time duration for sending two consecutive packets at the server.
  // It should be 0 for StartPacket test or NonPacedPacket test. For
  // PacedPacket test, it is derived from the inter_arrival_time_ in the
  // previous (StartPacket) test. For NATBind test, it is randomly generated
  // between 1 second and |maximum_NAT_idle_seconds_| seconds.
  base::TimeDelta pacing_interval_;
  // A list of tests that will be performed in sequence.
  std::vector<TestType> test_sequence_;
  uint32 current_test_index_;  // Index of the current test.

  ProbeMessage probe_message_;

  // Token received from server for authentication.
  ProbePacket_Token token_;

  // The state variables to track pending reads/writes.
  ReadState read_state_;
  WriteState write_state_;

  // We use this factory to create timeout tasks for socket's ReadData.
  base::WeakPtrFactory<NetworkStats> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStats);
};

class ProxyDetector {
 public:
  // Used for the callback that is called from |OnResolveProxyComplete|.
  typedef base::Callback<void(bool)> OnResolvedCallback;

  // Construct a ProxyDetector object that finds out if access to
  // |server_address| goes through a proxy server or not. Calls the |callback|
  // after proxy resolution is completed by currying the proxy resolution
  // status.
  ProxyDetector(net::ProxyService* proxy_service,
                const net::HostPortPair& server_address,
                OnResolvedCallback callback);

  // This method uses |proxy_service_| to resolve the proxy for
  // |server_address_|.
  void StartResolveProxy();

 private:
  // This object is deleted from |OnResolveProxyComplete|.
  ~ProxyDetector();

  // Call the |callback_| by currying the proxy resolution status.
  void OnResolveProxyComplete(int result);

  // |proxy_service_| specifies the proxy service that is to be used to find
  // if access to |server_address_| goes through proxy server or not.
  net::ProxyService* proxy_service_;

  // |server_address_| specifies the server host and port pair for which we are
  // trying to see if access to it, goes through proxy or not.
  net::HostPortPair server_address_;

  // |callback_| will be called after proxy resolution is completed.
  OnResolvedCallback callback_;

  // |proxy_info_| holds proxy information returned by ResolveProxy.
  net::ProxyInfo proxy_info_;

  // Indicate if there is a pending a proxy resolution. We use this to assert
  // that there is no in-progress proxy resolution request.
  bool has_pending_proxy_resolution_;
  DISALLOW_COPY_AND_ASSIGN(ProxyDetector);
};

// This collects the network connectivity stats for UDP protocol for small
// percentage of users who are participating in the experiment (by enabling
// "UMA upload"). This method gets called only if UMA upload to the
// server has succeeded.
void CollectNetworkStats(const std::string& network_stats_server_url,
                         IOThread* io_thread);

// This starts a series of tests randomly selected among one of the three
// choices of probe packet sizes: 100 Bytes, 500 Bytes, 1200 Bytes.
void StartNetworkStatsTest(net::HostResolver* host_resolver,
                           const net::HostPortPair& server_address,
                           uint16 histogram_port,
                           bool has_proxy_server);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NETWORK_STATS_H_
