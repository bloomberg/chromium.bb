// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_dns_client.h"

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sys_byteorder.h"
#include "base/test/test_timeouts.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/cert/merkle_tree_leaf.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/log/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {
namespace {

using ::testing::IsNull;
using ::testing::NotNull;
using net::test::IsError;
using net::test::IsOk;

constexpr char kLeafHash[] =
    "\x1f\x25\xe1\xca\xba\x4f\xf9\xb8\x27\x24\x83\x0f\xca\x60\xe4\xc2\xbe\xa8"
    "\xc3\xa9\x44\x1c\x27\xb0\xb4\x3e\x6a\x96\x94\xc7\xb8\x04";

// Always return min, to simplify testing.
// This should result in the DNS query ID always being 0.
int FakeRandInt(int min, int max) {
  return min;
}

std::vector<char> CreateDnsTxtRequest(base::StringPiece qname) {
  std::string encoded_qname;
  EXPECT_TRUE(net::DNSDomainFromDot(qname, &encoded_qname));

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

std::vector<std::string> GetSampleAuditProof(size_t length) {
  std::vector<std::string> audit_proof(length);
  // Makes each node of the audit proof different, so that tests are able to
  // confirm that the audit proof is reconstructed in the correct order.
  for (size_t i = 0; i < length; ++i) {
    std::string node(crypto::kSHA256Length, '\0');
    // Each node is 32 bytes, with each byte having a different value.
    for (size_t j = 0; j < crypto::kSHA256Length; ++j) {
      node[j] = static_cast<char>((-127 + i + j) % 128);
    }
    audit_proof[i].assign(std::move(node));
  }

  return audit_proof;
}

class MockLeafIndexCallback {
 public:
  MockLeafIndexCallback() : called_(false) {}

  bool called() const { return called_; }
  int net_error() const { return net_error_; }
  uint64_t leaf_index() const { return leaf_index_; }

  void Run(int net_error, uint64_t leaf_index) {
    EXPECT_TRUE(!called_);
    called_ = true;
    net_error_ = net_error;
    leaf_index_ = leaf_index;
    run_loop_.Quit();
  }

  LogDnsClient::LeafIndexCallback AsCallback() {
    return base::Bind(&MockLeafIndexCallback::Run, base::Unretained(this));
  }

  void WaitUntilRun() { run_loop_.Run(); }

 private:
  bool called_;
  int net_error_;
  uint64_t leaf_index_;
  base::RunLoop run_loop_;
};

class MockAuditProofCallback {
 public:
  MockAuditProofCallback() : called_(false) {}

  bool called() const { return called_; }
  int net_error() const { return net_error_; }
  const net::ct::MerkleAuditProof* proof() const { return proof_.get(); }

  void Run(int net_error, std::unique_ptr<net::ct::MerkleAuditProof> proof) {
    EXPECT_TRUE(!called_);
    called_ = true;
    net_error_ = net_error;
    proof_ = std::move(proof);
    run_loop_.Quit();
  }

  LogDnsClient::AuditProofCallback AsCallback() {
    return base::Bind(&MockAuditProofCallback::Run, base::Unretained(this));
  }

  void WaitUntilRun() { run_loop_.Run(); }

 private:
  bool called_;
  int net_error_;
  std::unique_ptr<net::ct::MerkleAuditProof> proof_;
  base::RunLoop run_loop_;
};

// A container for all of the data we need to keep alive for a mock socket.
// This is useful because Mock{Read,Write}, SequencedSocketData and
// MockClientSocketFactory all do not take ownership of or copy their arguments,
// so we have to manage the lifetime of those arguments ourselves. Wrapping all
// of that up in a single class simplifies this.
class MockSocketData {
 public:
  // A socket that expects one write and one read operation.
  MockSocketData(const std::vector<char>& write, const std::vector<char>& read)
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
                        eof_},
        socket_data_(expected_reads_, 2, &expected_write_, 1) {}

  // A socket that expects one write and a read error.
  MockSocketData(const std::vector<char>& write, int net_error)
      : expected_write_payload_(write),
        expected_write_(net::SYNCHRONOUS,
                        expected_write_payload_.data(),
                        expected_write_payload_.size(),
                        0),
        expected_reads_{net::MockRead(net::ASYNC, net_error, 1), eof_},
        socket_data_(expected_reads_, 2, &expected_write_, 1) {}

  // A socket that expects one write and no response.
  explicit MockSocketData(const std::vector<char>& write)
      : expected_write_payload_(write),
        expected_write_(net::SYNCHRONOUS,
                        expected_write_payload_.data(),
                        expected_write_payload_.size(),
                        0),
        expected_reads_{net::MockRead(net::SYNCHRONOUS, net::ERR_IO_PENDING, 1),
                        eof_},
        socket_data_(expected_reads_, 2, &expected_write_, 1) {}

  void SetWriteMode(net::IoMode mode) { expected_write_.mode = mode; }
  void SetReadMode(net::IoMode mode) { expected_reads_[0].mode = mode; }

  void AddToFactory(net::MockClientSocketFactory* socket_factory) {
    socket_factory->AddSocketDataProvider(&socket_data_);
  }

 private:
  // Prevents read overruns and makes a socket timeout the default behaviour.
  static const net::MockRead eof_;

  const std::vector<char> expected_write_payload_;
  const std::vector<char> expected_read_payload_;
  // Encapsulates the data that is expected to be written to a socket.
  net::MockWrite expected_write_;
  // Encapsulates the data/error that should be returned when reading from a
  // socket. The expected response is followed by |eof_|, to catch further,
  // unexpected read attempts.
  net::MockRead expected_reads_[2];
  net::SequencedSocketData socket_data_;

  DISALLOW_COPY_AND_ASSIGN(MockSocketData);
};

const net::MockRead MockSocketData::eof_(net::SYNCHRONOUS,
                                         net::ERR_IO_PENDING,
                                         2);

class LogDnsClientTest : public ::testing::TestWithParam<net::IoMode> {
 protected:
  LogDnsClientTest() {
    // Use an invalid nameserver address. This prevents the tests accidentally
    // sending real DNS queries. The mock sockets don't care that the address
    // is invalid.
    dns_config_.nameservers.push_back(net::IPEndPoint());
    // Don't attempt retransmissions - just fail.
    dns_config_.attempts = 1;
    // This ensures timeouts are long enough for memory tests.
    dns_config_.timeout = TestTimeouts::action_timeout();
    // Simplify testing - don't require random numbers for the source port.
    // This means our FakeRandInt function should only be called to get query
    // IDs.
    dns_config_.randomize_ports = false;
  }

  void ExpectRequestAndErrorResponse(base::StringPiece qname, uint8_t rcode) {
    std::vector<char> request = CreateDnsTxtRequest(qname);
    std::vector<char> response = CreateDnsErrorResponse(request, rcode);

    mock_socket_data_.emplace_back(new MockSocketData(request, response));
    mock_socket_data_.back()->SetReadMode(GetParam());
    mock_socket_data_.back()->AddToFactory(&socket_factory_);
  }

  void ExpectRequestAndSocketError(base::StringPiece qname, int net_error) {
    std::vector<char> request = CreateDnsTxtRequest(qname);

    mock_socket_data_.emplace_back(new MockSocketData(request, net_error));
    mock_socket_data_.back()->SetReadMode(GetParam());
    mock_socket_data_.back()->AddToFactory(&socket_factory_);
  }

  void ExpectRequestAndTimeout(base::StringPiece qname) {
    std::vector<char> request = CreateDnsTxtRequest(qname);

    mock_socket_data_.emplace_back(new MockSocketData(request));
    mock_socket_data_.back()->SetReadMode(GetParam());
    mock_socket_data_.back()->AddToFactory(&socket_factory_);

    // Speed up timeout tests.
    dns_config_.timeout = TestTimeouts::tiny_timeout();
  }

  void ExpectLeafIndexRequestAndResponse(base::StringPiece qname,
                                         base::StringPiece leaf_index) {
    // Prepend size to leaf_index to create the query answer (rdata)
    ASSERT_LE(leaf_index.size(), 0xFFul);  // size must fit into a single byte
    std::string answer = leaf_index.as_string();
    answer.insert(answer.begin(), static_cast<char>(leaf_index.size()));

    ExpectRequestAndResponse(qname, answer);
  }

  void ExpectAuditProofRequestAndResponse(
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

  void QueryLeafIndex(base::StringPiece log_domain,
                      base::StringPiece leaf_hash,
                      MockLeafIndexCallback* callback) {
    std::unique_ptr<net::DnsClient> dns_client = CreateDnsClient();
    LogDnsClient log_client(std::move(dns_client), net::BoundNetLog());

    log_client.QueryLeafIndex(log_domain, leaf_hash, callback->AsCallback());
    callback->WaitUntilRun();
  }

  void QueryAuditProof(base::StringPiece log_domain,
                       uint64_t leaf_index,
                       uint64_t tree_size,
                       MockAuditProofCallback* callback) {
    std::unique_ptr<net::DnsClient> dns_client = CreateDnsClient();
    LogDnsClient log_client(std::move(dns_client), net::BoundNetLog());

    log_client.QueryAuditProof(log_domain, leaf_index, tree_size,
                               callback->AsCallback());
    callback->WaitUntilRun();
  }

 private:
  std::unique_ptr<net::DnsClient> CreateDnsClient() {
    std::unique_ptr<net::DnsClient> client =
        net::DnsClient::CreateClientForTesting(nullptr, &socket_factory_,
                                               base::Bind(&FakeRandInt));
    client->SetConfig(dns_config_);
    return client;
  }

  void ExpectRequestAndResponse(base::StringPiece qname,
                                base::StringPiece answer) {
    std::vector<char> request = CreateDnsTxtRequest(qname);
    std::vector<char> response = CreateDnsTxtResponse(request, answer);

    mock_socket_data_.emplace_back(new MockSocketData(request, response));
    mock_socket_data_.back()->SetReadMode(GetParam());
    mock_socket_data_.back()->AddToFactory(&socket_factory_);
  }

  net::DnsConfig dns_config_;
  base::MessageLoopForIO message_loop_;
  std::vector<std::unique_ptr<MockSocketData>> mock_socket_data_;
  net::MockClientSocketFactory socket_factory_;
};

TEST_P(LogDnsClientTest, QueryLeafIndex) {
  ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      "123456");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsOk());
  EXPECT_THAT(callback.leaf_index(), 123456);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsThatLogDomainDoesNotExist) {
  ExpectRequestAndErrorResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::dns_protocol::kRcodeNXDOMAIN);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsServerFailure) {
  ExpectRequestAndErrorResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::dns_protocol::kRcodeSERVFAIL);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsServerRefusal) {
  ExpectRequestAndErrorResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::dns_protocol::kRcodeREFUSED);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexIsNotNumeric) {
  ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      "foo");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexIsFloatingPoint) {
  ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      "123456.0");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexIsEmpty) {
  ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.", "");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexHasNonNumericPrefix) {
  ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      "foo123456");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexHasNonNumericSuffix) {
  ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      "123456foo");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLogDomainIsEmpty) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLogDomainIsNull) {
  MockLeafIndexCallback callback;
  QueryLeafIndex(nullptr, kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLeafHashIsInvalid) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", "foo", &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLeafHashIsEmpty) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", "", &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLeafHashIsNull) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", nullptr, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsSocketError) {
  ExpectRequestAndSocketError(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::ERR_CONNECTION_REFUSED);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_CONNECTION_REFUSED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsTimeout) {
  ExpectRequestAndTimeout(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_TIMED_OUT));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryAuditProof) {
  const std::vector<std::string> audit_proof = GetSampleAuditProof(20);

  // It should require 3 queries to collect the entire audit proof, as there is
  // only space for 7 nodes per UDP packet.
  ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                     audit_proof.begin(),
                                     audit_proof.begin() + 7);
  ExpectAuditProofRequestAndResponse("7.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 7,
                                     audit_proof.begin() + 14);
  ExpectAuditProofRequestAndResponse("14.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 14,
                                     audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsOk());
  ASSERT_THAT(callback.proof(), NotNull());
  EXPECT_THAT(callback.proof()->leaf_index, 123456);
  // EXPECT_THAT(callback.proof()->tree_size, 999999);
  EXPECT_THAT(callback.proof()->nodes, audit_proof);
}

TEST_P(LogDnsClientTest, QueryAuditProofHandlesResponsesWithShortAuditPaths) {
  const std::vector<std::string> audit_proof = GetSampleAuditProof(20);

  // Make some of the responses contain fewer proof nodes than they can hold.
  ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                     audit_proof.begin(),
                                     audit_proof.begin() + 1);
  ExpectAuditProofRequestAndResponse("1.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 1,
                                     audit_proof.begin() + 3);
  ExpectAuditProofRequestAndResponse("3.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 3,
                                     audit_proof.begin() + 6);
  ExpectAuditProofRequestAndResponse("6.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 6,
                                     audit_proof.begin() + 10);
  ExpectAuditProofRequestAndResponse("10.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 10,
                                     audit_proof.begin() + 13);
  ExpectAuditProofRequestAndResponse("13.123456.999999.tree.ct.test.",
                                     audit_proof.begin() + 13,
                                     audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsOk());
  ASSERT_THAT(callback.proof(), NotNull());
  EXPECT_THAT(callback.proof()->leaf_index, 123456);
  // EXPECT_THAT(callback.proof()->tree_size, 999999);
  EXPECT_THAT(callback.proof()->nodes, audit_proof);
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsThatLogDomainDoesNotExist) {
  ExpectRequestAndErrorResponse("0.123456.999999.tree.ct.test.",
                                net::dns_protocol::kRcodeNXDOMAIN);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsServerFailure) {
  ExpectRequestAndErrorResponse("0.123456.999999.tree.ct.test.",
                                net::dns_protocol::kRcodeSERVFAIL);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsServerRefusal) {
  ExpectRequestAndErrorResponse("0.123456.999999.tree.ct.test.",
                                net::dns_protocol::kRcodeREFUSED);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest,
       QueryAuditProofReportsResponseMalformedIfNodeTooShort) {
  // node is shorter than a SHA-256 hash (31 vs 32 bytes)
  const std::vector<std::string> audit_proof(1, std::string(31, 'a'));

  ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                     audit_proof.begin(), audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsResponseMalformedIfNodeTooLong) {
  // node is longer than a SHA-256 hash (33 vs 32 bytes)
  const std::vector<std::string> audit_proof(1, std::string(33, 'a'));

  ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                     audit_proof.begin(), audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsResponseMalformedIfEmpty) {
  const std::vector<std::string> audit_proof;

  ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                     audit_proof.begin(), audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsInvalidArgIfLogDomainIsEmpty) {
  MockAuditProofCallback callback;
  QueryAuditProof("", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsInvalidArgIfLogDomainIsNull) {
  MockAuditProofCallback callback;
  QueryAuditProof(nullptr, 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest,
       QueryAuditProofReportsInvalidArgIfLeafIndexEqualToTreeSize) {
  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 123456, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest,
       QueryAuditProofReportsInvalidArgIfLeafIndexGreaterThanTreeSize) {
  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 999999, 123456, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsSocketError) {
  ExpectRequestAndSocketError("0.123456.999999.tree.ct.test.",
                              net::ERR_CONNECTION_REFUSED);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_CONNECTION_REFUSED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsTimeout) {
  ExpectRequestAndTimeout("0.123456.999999.tree.ct.test.");

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_TIMED_OUT));
  EXPECT_THAT(callback.proof(), IsNull());
}

INSTANTIATE_TEST_CASE_P(ReadMode,
                        LogDnsClientTest,
                        ::testing::Values(net::IoMode::ASYNC,
                                          net::IoMode::SYNCHRONOUS));

}  // namespace
}  // namespace certificate_transparency
