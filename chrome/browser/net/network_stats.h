// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NETWORK_STATS_H_
#define CHROME_BROWSER_NET_NETWORK_STATS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/io_thread.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/test_data_stream.h"
#include "net/dns/host_resolver.h"
#include "net/proxy/proxy_info.h"
#include "net/socket/socket.h"

namespace chrome_browser_net {

// This class is used for live experiment of network connectivity (for UDP)
// metrics. A small percentage of users participate in this experiment. All
// users (who are in the experiment) must have enabled "UMA upload".
//
// This class collects the following stats from users who have opted in.
// a) Success/failure, to estimate reachability for users.
// b) RTT for some select set of messages.
// c) Perform packet loss tests for correlation and FEC experiments.
//
// The following is the overview of the echo message protocol.
//
// We send the "echo request" to the UDP echo servers in the following format:
// <version><checksum><payload_size><payload>. <version> is the version number
// of the "echo request". <checksum> is the checksum of the <payload>.
// <payload_size> specifies the number of bytes in the <payload>.
//
// UDP echo servers respond to the "echo request" by returning "echo response".
// "echo response" is of the format:
// "<version><checksum><payload_size><key><encoded_payload>". <payload_size>
// specifies the number of bytes in the <encoded_payload>. <key> is used to
// decode the <encoded_payload>.

class NetworkStats {
 public:
  enum Status {                 // Used in HISTOGRAM_ENUMERATION.
    SUCCESS,                    // Successfully received bytes from the server.
    IP_STRING_PARSE_FAILED,     // Parsing of IP string failed.
    SOCKET_CREATE_FAILED,       // Socket creation failed.
    RESOLVE_FAILED,             // Host resolution failed.
    CONNECT_FAILED,             // Connection to the server failed.
    WRITE_FAILED,               // Sending an echo message to the server failed.
    READ_TIMED_OUT,             // Reading the reply from the server timed out.
    READ_FAILED,                // Reading the reply from the server failed.
    ZERO_LENGTH_ERROR,          // Zero length message.
    NO_CHECKSUM_ERROR,          // Message doesn't have a checksum.
    NO_KEY_ERROR,               // Message doesn't have a key.
    NO_PAYLOAD_SIZE_ERROR,      // Message doesn't have a payload size.
    NO_PAYLOAD_ERROR,           // Message doesn't have a payload.
    INVALID_KEY_ERROR,          // Invalid key in the message.
    TOO_SHORT_PAYLOAD,          // Message is shorter than payload.
    TOO_LONG_PAYLOAD,           // Message is longer than payload.
    INVALID_CHECKSUM,           // Checksum verification failed.
    PATTERN_CHANGED,            // Pattern in payload has changed.
    INVALID_PACKET_NUMBER,      // Packet number didn't match.
    TOO_MANY_PACKETS,           // Received more packets than the packets sent.
    PREVIOUS_PACKET_NUMBER,     // Received a packet from an earlier test.
    DUPLICATE_PACKET,           // Received a duplicate packet.
    SOME_PACKETS_NOT_VERIFIED,  // Some packets have failed verification.
    STATUS_MAX,                 // Bounding value.
  };

  // |ProtocolValue| enumerates different protocols that are being tested.
  enum ProtocolValue {
    PROTOCOL_UDP,
  };

  // |HistogramPortSelector| enumerates list of ports that are used for network
  // connectivity tests (for UDP). Currently we are testing port 6121 only.
  enum HistogramPortSelector {
    PORT_6121 = 0,    // SPDY
    HISTOGRAM_PORT_MAX,
  };

  // |TestType| specifies the |current_test_| test we are currently running or
  // the |next_test_| test we need to run next.
  enum TestType {
    START_PACKET_TEST,      // Initial packet loss test.
    NON_PACED_PACKET_TEST,  // Packet loss test with no pacing.
    PACED_PACKET_TEST,      // Packet loss test with pacing.
    TEST_TYPE_MAX,
  };

  // |PacketStatus| collects the Status and RTT statistics for each packet.
  struct PacketStatus {
    base::TimeTicks start_time_;
    base::TimeTicks end_time_;
  };

  // Constructs a NetworkStats object that collects metrics for network
  // connectivity (for UDP).
  NetworkStats();
  // NetworkStats is deleted when Finish() is called.
  ~NetworkStats();

  // Starts the client, connecting to |server|.
  // Client will send |bytes_to_send| bytes to |server|.
  // When client has received all echoed bytes from the server, or
  // when an error occurs causing the client to stop, |Finish| will be
  // called with a net status code.
  // |Finish| will collect histogram stats.
  // Returns true if successful in starting the client.
  bool Start(net::HostResolver* host_resolver,
             const net::HostPortPair& server,
             HistogramPortSelector histogram_port,
             bool has_proxy_server,
             uint32 bytes_to_send,
             uint32 packets_to_send,
             const net::CompletionCallback& callback);

 private:
  friend class NetworkStatsTest;

  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTest, GetHistogramNames);
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTestUDP, VerifyBytesInvalidHeaders);
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTestUDP, VerifyBytesInvalidChecksum);
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTestUDP, VerifyBytesPreviousPacket);
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTestUDP, VerifyBytesInvalidPacketNumber);
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTestUDP, VerifyBytesPatternChanged);
  FRIEND_TEST_ALL_PREFIXES(NetworkStatsTestUDP, VerifyBytes);

  // Starts the test specified by the |next_test_|. It also resets all the book
  // keeping data, before starting the new test.
  void RestartPacketTest();

  // Initializes |finished_callback_| and the number of bytes to send to the
  // server. |finished_callback| is called when we are done with the test.
  // |finished_callback| is mainly useful for unittests.
  void Initialize(uint32 bytes_to_send,
                  HistogramPortSelector histogram_port,
                  bool has_proxy_server,
                  uint32 packets_to_send,
                  const net::CompletionCallback& finished_callback);

  // Resets all the counters and the collected stats.
  void ResetData();

  // Callback that is called when host resolution is completed.
  void OnResolveComplete(int result);

  // Called after host is resolved. Creates UDPClientSocket and connects to the
  // server. If successfully connected, then calls ConnectComplete() to start
  // the network connectivity tests. Returns |false| if there is any error.
  bool DoConnect(int result);

  // This method is called after socket connection is completed. It will start
  // the process of sending packets to |server| by calling SendPacket(). Returns
  // false if connection is not established (result is less than 0).
  bool ConnectComplete(int result);

  // This method is called whenever we need to send a packet. It is called from
  // either ConnectComplete or OnWriteComplete. It will send a packet, based on
  // load_size_, to |server| by calling SendData(). If there are no more packets
  // to send, it calls ReadData() to read/verify the data from the |server|.
  void SendPacket();

  // Sends the next packet by calling |SendPacket| after a delay. Delay time is
  // calculated based on the remaining packets and the remaining time.
  // For START_PACKET_TEST and NON_PACED_PACKET_TEST delay is zero. It also
  // yields for packets received between sends.
  void SendNextPacketAfterDelay();

  // Verifies the data and calls Finish() if there is a significant network
  // error or if all packets are sent. Returns true if Finish() is called
  // otherwise returns false.
  bool ReadComplete(int result);

  // Callbacks when an internal IO is completed.
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  // Reads data from server until an error occurs.
  void ReadData();

  // Sends data to server until an error occurs.
  int SendData();

  // Determine the size of the packet from |load_size_|. The packet size
  // includes |load_size_| plus the header size.
  uint32 SendingPacketSize() const;
  uint32 ReceivingPacketSize() const;

  // This method decrements the |bytes_to_send_| by the |bytes_sent| and updates
  // |packets_sent_| if all the bytes are sent. It also informs |write_buffer_|
  // that data has been consumed.
  void DidSendData(int bytes_sent);

  // We set a timeout for responses from the echo servers.
  void StartReadDataTimer(int milliseconds);

  // Called when the ReadData Timer fires. |test_base_packet_number| specifies
  // the |base_packet_number_| when we have started the timer. If we haven't
  // received all the packets for the test (test took longer than 30 secs to
  // run), then we call Finish() to finish processing. If we have already
  // received all packets from the server, then this method is a no-op.
  void OnReadDataTimeout(uint32 test_base_packet_number);

  // Returns the checksum for the message.
  uint32 GetChecksum(const char* message, uint32 message_length);

  // Encrypts/decrypts the data with the key and returns encrypted/decrypted
  // data in |encoded_data|.
  void Crypt(const char* key,
             uint32 key_length,
             const char* data,
             uint32 data_length,
             char* encoded_data);

  // Fills the |io_buffer| with the "echo request" message. This gets the
  // <payload> from |stream_| and calculates the <checksum> of the <payload> and
  // returns the "echo request" that has <version>, <checksum>, <payload_size>
  // and <payload>. Every <payload> has a unique packet number stored in it.
  void GetEchoRequest(net::IOBufferWithSize* io_buffer);

  // This method verifies that we have received all the packets we have sent. It
  // verifies the |encoded_message_| by calling VerifyBytes() for each packet
  // that is in it. It returns SUCCESS, if all the packets are verified.
  NetworkStats::Status VerifyPackets();

  // This method parses the "echo response" message in the |response| to verify
  // that the <payload> is same as what we had sent in "echo request" message.
  // As it verifies the response in each packet, it also extracts the packet
  // number, and records that said packet number responded. It returns SUCCESS,
  // if all the bytes are verified.
  NetworkStats::Status VerifyBytes(const std::string& response);

  // Collects network connectivity stats. This is called when all the data from
  // server is read or when there is a failure during connect/read/write. It
  // will either restart the second phase of the test, or it will self destruct
  // at the end of this method.
  void Finish(Status status, int result);

  // This method is called from Finish() and calls |finished_callback_| callback
  // to indicate that the test has finished.
  void DoFinishCallback(int result);

  // Collect the network connectivity stats by calling RecordRTTHistograms(),
  // RecordAcksReceivedHistograms() and RecordPacketLossSeriesHistograms(). See
  // below for details.
  void RecordHistograms(const ProtocolValue& protocol,
                        const Status& status,
                        int result);

  // Collect the following network connectivity stats when
  // kMaximumSequentialPackets (21) packets are sent.
  // a) Received the "echo response" for at least one packet.
  // b) Received the "echo response" for the nth packet.
  // c) Count the number of "echo responses" received for each of the initial
  // sequences of packets 1...n.
  void RecordAcksReceivedHistograms(const std::string& load_size_string);

  // Collect the following network connectivity stats for
  // kMaximumCorrelationPackets (6) packets test.
  // a) Success/failure of each packet, to estimate reachability for users.
  // b) Records if there is a probabalistic dependency in packet loss when
  // kMaximumCorrelationPackets packets are sent consecutively.
  void RecordPacketLossSeriesHistograms(const ProtocolValue& protocol,
                                        const std::string& load_size_string,
                                        const Status& status,
                                        int result);

  // Collects the RTT for the packet specified by the |index|.
  void RecordRTTHistograms(const ProtocolValue& protocol,
                           const std::string& load_size_string,
                           uint32 index);

  uint32 load_size() const { return load_size_; }

  // Returns string representation of test_type_.
  const char* TestName();

  uint32 packet_number() const { return packet_number_; }
  uint32 base_packet_number() const { return base_packet_number_; }
  uint32 set_base_packet_number(uint32 packet_number) {
    return base_packet_number_ = packet_number;
  }

  net::Socket* socket() { return socket_.get(); }
  void set_socket(net::Socket* socket);

  const net::AddressList& addresses() const { return addresses_; }

  uint32 packets_received_mask() const { return packets_received_mask_; }

  TestType next_test() const { return next_test_; }

  // The socket handle for this session.
  scoped_ptr<net::Socket> socket_;

  // The read buffer used to read data from the socket.
  scoped_refptr<net::IOBuffer> read_buffer_;

  // The write buffer used to write data to the socket.
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  // Some counters for the session.
  uint32 load_size_;
  uint32 bytes_to_read_;
  uint32 bytes_to_send_;

  // |stream_| is used to generate data to be sent to the server and it is also
  // used to verify the data received from the server.
  net::TestDataStream stream_;

  // |histogram_port_| specifies the port for which we are testing the network
  // connectivity.
  HistogramPortSelector histogram_port_;

  // |has_proxy_server_| specifies if there is a proxy server or not.
  bool has_proxy_server_;

  // HostResolver fills out the |addresses_| after host resolution is completed.
  net::AddressList addresses_;

  // Callback to call when echo protocol is successefully finished or whenever
  // there is an error (this allows unittests to wait until echo protocol's
  // round trip is finished).
  net::CompletionCallback finished_callback_;

  // Collects the Status and RTT for each packet.
  std::vector<PacketStatus> packet_status_;

  // The time when we have received 1st byte from the server.
  base::TimeTicks packet_1st_byte_read_time_;

  // The last time when we have received data from the server.
  base::TimeTicks packet_last_byte_read_time_;

  // This is the average time it took to receive a packet from the server.
  base::TimeDelta average_time_;

  // Data to track number of packets to send to the server and the packets we
  // have received from the server.
  uint32 packets_to_send_;        // Numbers of packets that are to be sent.
  uint32 packets_sent_;           // Numbers of packets sent to the server.
  uint32 packets_received_;       // Number of packets successfully received.
  uint32 packets_received_mask_;  // Tracks the received status of each packet.
  uint32 packet_number_;          // The packet number being sent to the server.
  uint32 base_packet_number_;     // The packet number before the test.
  bool sending_complete_;         // Finished sending all packets.

  TestType current_test_;  // Current test that is running.
  TestType next_test_;     // Next test that is to be run.

  // We use this factory to create timeout tasks for socket's ReadData.
  base::WeakPtrFactory<NetworkStats> weak_factory_;
};

class ProxyDetector {
 public:
  // Used for the callback that is called from |OnResolveProxyComplete|.
  typedef base::Callback<void(bool)> OnResolvedCallback;

  // Constructs a ProxyDetector object that finds out if access to
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

  // Calls the |callback_| by currying the proxy resolution status.
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

  // Indicates if there is a pending a proxy resolution. We use this to assert
  // that there is no in-progress proxy resolution request.
  bool has_pending_proxy_resolution_;
};

// This collects the network connectivity stats for UDP protocol for small
// percentage of users who are participating in the experiment. All users must
// have enabled "UMA upload". This method gets called only if UMA upload to the
// server has succeeded.
void CollectNetworkStats(const std::string& network_stats_server_url,
                         IOThread* io_thread);

// This starts a test randomly selected among "6 packets correlation test for
// 1200 bytes packet", "21 packets bursty test with 1200 bytes packet",
// "21 packets bursty test with 500 bytes packet", and "21 packets bursty test
// with 100 bytes packet" tests.
void StartNetworkStatsTest(net::HostResolver* host_resolver,
                           const net::HostPortPair& server_address,
                           NetworkStats::HistogramPortSelector histogram_port,
                           bool has_proxy_server);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_NETWORK_STATS_H_
