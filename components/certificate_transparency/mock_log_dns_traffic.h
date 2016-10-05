// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_MOCK_LOG_DNS_TRAFFIC_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_MOCK_LOG_DNS_TRAFFIC_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/socket/socket_test_util.h"

namespace certificate_transparency {

namespace internal {

// A container for all of the data we need to keep alive for a mock socket.
// This is useful because Mock{Read,Write}, SequencedSocketData and
// MockClientSocketFactory all do not take ownership of or copy their arguments,
// so we have to manage the lifetime of those arguments ourselves. Wrapping all
// of that up in a single class simplifies this.
// This cannot be forward declared because MockLogDnsTraffic has a
// vector<unique_ptr<MockSocketData>> member, which requires MockSocketData be
// defined.
class MockSocketData {
 public:
  // A socket that expects one write and one read operation.
  MockSocketData(const std::vector<char>& write, const std::vector<char>& read);
  // A socket that expects one write and a read error.
  MockSocketData(const std::vector<char>& write, int net_error);
  // A socket that expects one write and no response.
  explicit MockSocketData(const std::vector<char>& write);

  ~MockSocketData();

  void SetWriteMode(net::IoMode mode) { expected_write_.mode = mode; }
  void SetReadMode(net::IoMode mode) { expected_reads_[0].mode = mode; }

  void AddToFactory(net::MockClientSocketFactory* socket_factory);

 private:
  // Prevents read overruns and makes a socket timeout the default behaviour.
  static const net::MockRead no_more_data_;

  // This class only supports one write and one read, so just need to store one
  // payload each.
  const std::vector<char> expected_write_payload_;
  const std::vector<char> expected_read_payload_;
  // Encapsulates the data that is expected to be written to a socket.
  net::MockWrite expected_write_;
  // Encapsulates the data/error that should be returned when reading from a
  // socket. The second "expected" read is always |no_more_data_|, which
  // causes the socket read to hang until it times out. This results in better
  // test failure messages (rather than a CHECK-fail due to a socket read
  // overrunning the MockRead array) and behaviour more like a real socket when
  // an unexpected second socket read occurs.
  net::MockRead expected_reads_[2];
  // Holds pointers to |expected_write_| and |expected_reads_|. This is what is
  // added to net::MockClientSocketFactory to prepare a mock socket.
  net::SequencedSocketData socket_data_;

  DISALLOW_COPY_AND_ASSIGN(MockSocketData);
};

}  // namespace internal

// Mocks DNS requests and responses for a Certificate Transparency (CT) log.
// This is implemented using mock sockets. Call the CreateDnsClient() method to
// get a net::DnsClient wired up to these mock sockets.
// The Expect*() methods must be called from within a GTest test case.
class MockLogDnsTraffic {
 public:
  MockLogDnsTraffic();
  ~MockLogDnsTraffic();

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS response indicating that the error
  // specified by |rcode| occurred. See RFC1035, Section 4.1.1 for |rcode|
  // values.
  void ExpectRequestAndErrorResponse(base::StringPiece qname, uint8_t rcode);
  // Expect a CT DNS request for the domain |qname|.
  // Such a request will trigger a socket error of type |net_error|.
  // |net_error| can be any net:Error value.
  void ExpectRequestAndSocketError(base::StringPiece qname, int net_error);
  // Expect a CT DNS request for the domain |qname|.
  // Such a request will timeout.
  // This will reduce the DNS timeout to minimize test duration.
  void ExpectRequestAndTimeout(base::StringPiece qname);
  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS TXT response containing |txt_strings|.
  void ExpectRequestAndResponse(
    base::StringPiece qname,
    const std::vector<base::StringPiece>& txt_strings);
  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS response containing |leaf_index|.
  // A description of such a request and response can be seen here:
  // https://github.com/google/certificate-transparency-rfcs/blob/c8844de6bd0b5d3d16bac79865e6edef533d760b/dns/draft-ct-over-dns.md#hash-query-hashquery
  void ExpectLeafIndexRequestAndResponse(base::StringPiece qname,
                                         uint64_t leaf_index);
  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS response containing the inclusion proof
  // nodes between |audit_path_start| and |audit_path_end|.
  // A description of such a request and response can be seen here:
  // https://github.com/google/certificate-transparency-rfcs/blob/c8844de6bd0b5d3d16bac79865e6edef533d760b/dns/draft-ct-over-dns.md#tree-query-treequery
  void ExpectAuditProofRequestAndResponse(
      base::StringPiece qname,
      std::vector<std::string>::const_iterator audit_path_start,
      std::vector<std::string>::const_iterator audit_path_end);

  // Sets the initial DNS config appropriate for testing.
  // Requires that net::NetworkChangeNotifier is initialized first.
  // The DNS config is propogated to NetworkChangeNotifier::DNSObservers
  // asynchronously.
  void InitializeDnsConfig();

  // Sets the DNS config to |config|.
  // Requires that net::NetworkChangeNotifier is initialized first.
  // The DNS config is propogated to NetworkChangeNotifier::DNSObservers
  // asynchronously.
  void SetDnsConfig(const net::DnsConfig& config);

  // Creates a DNS client that uses mock sockets.
  // It is this DNS client that the expectations will be tested against.
  std::unique_ptr<net::DnsClient> CreateDnsClient();

  // Sets whether mock reads should complete synchronously or asynchronously.
  void SetSocketReadMode(net::IoMode read_mode) {
    socket_read_mode_ = read_mode;
  }

 private:
  // Constructs MockSocketData from |args| and adds it to |socket_factory_|.
  template <typename... Args>
  void EmplaceMockSocketData(Args&&... args);

  // Sets the timeout used for DNS queries.
  // Requires that net::NetworkChangeNotifier is initialized first.
  // The new timeout is propogated to NetworkChangeNotifier::DNSObservers
  // asynchronously.
  void SetDnsTimeout(const base::TimeDelta& timeout);

  // One MockSocketData for each socket that is created. This corresponds to one
  // for each DNS request sent.
  std::vector<std::unique_ptr<internal::MockSocketData>> mock_socket_data_;
  // Provides as many mock sockets as there are entries in |mock_socket_data_|.
  net::MockClientSocketFactory socket_factory_;
  // Controls whether mock socket reads are asynchronous.
  net::IoMode socket_read_mode_;

  DISALLOW_COPY_AND_ASSIGN(MockLogDnsTraffic);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_MOCK_LOG_DNS_TRAFFIC_H_
