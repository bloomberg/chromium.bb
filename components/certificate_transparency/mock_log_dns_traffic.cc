// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/mock_log_dns_traffic.h"

#include <algorithm>
#include <numeric>
#include <vector>

#include "base/big_endian.h"
#include "base/sys_byteorder.h"
#include "base/test/test_timeouts.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_util.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {

// Necessary to expose SetDnsConfig for testing.
class DnsChangeNotifier : public net::NetworkChangeNotifier {
 public:
  static void SetInitialDnsConfig(const net::DnsConfig& config) {
    net::NetworkChangeNotifier::SetInitialDnsConfig(config);
  }

  static void SetDnsConfig(const net::DnsConfig& config) {
    net::NetworkChangeNotifier::SetDnsConfig(config);
  }
};

// Always return min, to simplify testing.
// This should result in the DNS query ID always being 0.
int FakeRandInt(int min, int max) {
  return min;
}

std::vector<char> CreateDnsTxtRequest(base::StringPiece qname) {
  std::string encoded_qname;
  EXPECT_TRUE(net::DNSDomainFromDot(qname, &encoded_qname));

  // DNS query section:
  // N bytes - qname
  // 2 bytes - record type
  // 2 bytes - record class
  // Total = N + 4 bytes
  const size_t query_section_size = encoded_qname.size() + 4;

  std::vector<char> request(sizeof(net::dns_protocol::Header) +
                            query_section_size);
  base::BigEndianWriter writer(request.data(), request.size());

  // Header
  net::dns_protocol::Header header = {};
  header.flags = base::HostToNet16(net::dns_protocol::kFlagRD);
  header.qdcount = base::HostToNet16(1);
  EXPECT_TRUE(writer.WriteBytes(&header, sizeof(header)));
  // Query section
  EXPECT_TRUE(writer.WriteBytes(encoded_qname.data(), encoded_qname.size()));
  EXPECT_TRUE(writer.WriteU16(net::dns_protocol::kTypeTXT));
  EXPECT_TRUE(writer.WriteU16(net::dns_protocol::kClassIN));
  EXPECT_EQ(0, writer.remaining());

  return request;
}

std::vector<char> CreateDnsTxtResponse(const std::vector<char>& request,
                                       base::StringPiece answer) {
  // DNS answers section:
  // 2 bytes - qname pointer
  // 2 bytes - record type
  // 2 bytes - record class
  // 4 bytes - time-to-live
  // 2 bytes - size of answer (N)
  // N bytes - answer
  // Total = 12 + N bytes
  const size_t answers_section_size = 12 + answer.size();
  constexpr uint32_t ttl = 86400;  // seconds

  std::vector<char> response(request.size() + answers_section_size);
  std::copy(request.begin(), request.end(), response.begin());
  // Modify the header
  net::dns_protocol::Header* header =
      reinterpret_cast<net::dns_protocol::Header*>(response.data());
  header->ancount = base::HostToNet16(1);
  header->flags |= base::HostToNet16(net::dns_protocol::kFlagResponse);

  // Write the answer section
  base::BigEndianWriter writer(response.data() + request.size(),
                               response.size() - request.size());
  EXPECT_TRUE(writer.WriteU8(0xc0));  // qname is a pointer
  EXPECT_TRUE(writer.WriteU8(
      sizeof(*header)));  // address of qname (start of query section)
  EXPECT_TRUE(writer.WriteU16(net::dns_protocol::kTypeTXT));
  EXPECT_TRUE(writer.WriteU16(net::dns_protocol::kClassIN));
  EXPECT_TRUE(writer.WriteU32(ttl));
  EXPECT_TRUE(writer.WriteU16(answer.size()));
  EXPECT_TRUE(writer.WriteBytes(answer.data(), answer.size()));
  EXPECT_EQ(0, writer.remaining());

  return response;
}

std::vector<char> CreateDnsErrorResponse(const std::vector<char>& request,
                                         uint8_t rcode) {
  std::vector<char> response(request);
  // Modify the header
  net::dns_protocol::Header* header =
      reinterpret_cast<net::dns_protocol::Header*>(response.data());
  header->ancount = base::HostToNet16(1);
  header->flags |= base::HostToNet16(net::dns_protocol::kFlagResponse | rcode);

  return response;
}

}  // namespace

namespace internal {

MockSocketData::MockSocketData(const std::vector<char>& write,
                               const std::vector<char>& read)
    : expected_write_payload_(write),
      expected_read_payload_(read),
      expected_write_(net::SYNCHRONOUS,
                      expected_write_payload_.data(),
                      expected_write_payload_.size(),
                      0),
      expected_reads_{net::MockRead(net::ASYNC,
                                    expected_read_payload_.data(),
                                    expected_read_payload_.size(),
                                    1),
                      no_more_data_},
      socket_data_(expected_reads_, 2, &expected_write_, 1) {}

MockSocketData::MockSocketData(const std::vector<char>& write, int net_error)
    : expected_write_payload_(write),
      expected_write_(net::SYNCHRONOUS,
                      expected_write_payload_.data(),
                      expected_write_payload_.size(),
                      0),
      expected_reads_{net::MockRead(net::ASYNC, net_error, 1), no_more_data_},
      socket_data_(expected_reads_, 2, &expected_write_, 1) {}

MockSocketData::MockSocketData(const std::vector<char>& write)
    : expected_write_payload_(write),
      expected_write_(net::SYNCHRONOUS,
                      expected_write_payload_.data(),
                      expected_write_payload_.size(),
                      0),
      expected_reads_{net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING, 1),
                      no_more_data_},
      socket_data_(expected_reads_, 2, &expected_write_, 1) {}

MockSocketData::~MockSocketData() {}

void MockSocketData::AddToFactory(
    net::MockClientSocketFactory* socket_factory) {
  socket_factory->AddSocketDataProvider(&socket_data_);
}

const net::MockRead MockSocketData::no_more_data_(net::SYNCHRONOUS,
                                                  net::ERR_IO_PENDING,
                                                  2);

}  // namespace internal

using internal::MockSocketData;

MockLogDnsTraffic::MockLogDnsTraffic() : socket_read_mode_(net::ASYNC) {}

MockLogDnsTraffic::~MockLogDnsTraffic() {}

void MockLogDnsTraffic::ExpectRequestAndErrorResponse(base::StringPiece qname,
                                                      uint8_t rcode) {
  std::vector<char> request = CreateDnsTxtRequest(qname);
  EmplaceMockSocketData(CreateDnsTxtRequest(qname),
                        CreateDnsErrorResponse(request, rcode));
}

void MockLogDnsTraffic::ExpectRequestAndSocketError(base::StringPiece qname,
                                                    int net_error) {
  EmplaceMockSocketData(CreateDnsTxtRequest(qname), net_error);
}

void MockLogDnsTraffic::ExpectRequestAndTimeout(base::StringPiece qname) {
  EmplaceMockSocketData(CreateDnsTxtRequest(qname));

  // Speed up timeout tests.
  SetDnsTimeout(TestTimeouts::tiny_timeout());
}

void MockLogDnsTraffic::ExpectLeafIndexRequestAndResponse(
    base::StringPiece qname,
    base::StringPiece leaf_index) {
  // Prepend size to leaf_index to create the query answer (rdata)
  ASSERT_LE(leaf_index.size(), 0xFFul);  // size must fit into a single byte
  std::string answer = leaf_index.as_string();
  answer.insert(answer.begin(), static_cast<char>(leaf_index.size()));

  ExpectRequestAndResponse(qname, answer);
}

void MockLogDnsTraffic::ExpectAuditProofRequestAndResponse(
    base::StringPiece qname,
    std::vector<std::string>::const_iterator audit_path_start,
    std::vector<std::string>::const_iterator audit_path_end) {
  // Join nodes in the audit path into a single string.
  std::string proof =
      std::accumulate(audit_path_start, audit_path_end, std::string());

  // Prepend size to proof to create the query answer (rdata)
  ASSERT_LE(proof.size(), 0xFFul);  // size must fit into a single byte
  proof.insert(proof.begin(), static_cast<char>(proof.size()));

  ExpectRequestAndResponse(qname, proof);
}

void MockLogDnsTraffic::InitializeDnsConfig() {
  net::DnsConfig dns_config;
  // Use an invalid nameserver address. This prevents the tests accidentally
  // sending real DNS queries. The mock sockets don't care that the address
  // is invalid.
  dns_config.nameservers.push_back(net::IPEndPoint());
  // Don't attempt retransmissions - just fail.
  dns_config.attempts = 1;
  // This ensures timeouts are long enough for memory tests.
  dns_config.timeout = TestTimeouts::action_timeout();
  // Simplify testing - don't require random numbers for the source port.
  // This means our FakeRandInt function should only be called to get query
  // IDs.
  dns_config.randomize_ports = false;

  DnsChangeNotifier::SetInitialDnsConfig(dns_config);
}

void MockLogDnsTraffic::SetDnsConfig(const net::DnsConfig& config) {
  DnsChangeNotifier::SetDnsConfig(config);
}

std::unique_ptr<net::DnsClient> MockLogDnsTraffic::CreateDnsClient() {
  return net::DnsClient::CreateClientForTesting(nullptr, &socket_factory_,
                                                base::Bind(&FakeRandInt));
}

void MockLogDnsTraffic::ExpectRequestAndResponse(base::StringPiece qname,
                                                 base::StringPiece answer) {
  std::vector<char> request = CreateDnsTxtRequest(qname);
  EmplaceMockSocketData(request, CreateDnsTxtResponse(request, answer));
}

template <typename... Args>
void MockLogDnsTraffic::EmplaceMockSocketData(Args&&... args) {
  mock_socket_data_.emplace_back(
      new MockSocketData(std::forward<Args>(args)...));
  mock_socket_data_.back()->SetReadMode(socket_read_mode_);
  mock_socket_data_.back()->AddToFactory(&socket_factory_);
}

void MockLogDnsTraffic::SetDnsTimeout(const base::TimeDelta& timeout) {
  net::DnsConfig dns_config;
  DnsChangeNotifier::GetDnsConfig(&dns_config);
  dns_config.timeout = timeout;
  DnsChangeNotifier::SetDnsConfig(dns_config);
}

}  // namespace certificate_transparency
