// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_dns_client.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/certificate_transparency/mock_log_dns_traffic.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/log/net_log.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {
namespace {

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using net::test::IsError;
using net::test::IsOk;

constexpr char kLeafHash[] =
    "\x1f\x25\xe1\xca\xba\x4f\xf9\xb8\x27\x24\x83\x0f\xca\x60\xe4\xc2\xbe\xa8"
    "\xc3\xa9\x44\x1c\x27\xb0\xb4\x3e\x6a\x96\x94\xc7\xb8\x04";

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

class LogDnsClientTest : public ::testing::TestWithParam<net::IoMode> {
 protected:
  LogDnsClientTest()
      : network_change_notifier_(net::NetworkChangeNotifier::CreateMock()) {
    mock_dns_.SetSocketReadMode(GetParam());
    mock_dns_.InitializeDnsConfig();
  }

  void QueryLeafIndex(base::StringPiece log_domain,
                      base::StringPiece leaf_hash,
                      MockLeafIndexCallback* callback) {
    LogDnsClient log_client(mock_dns_.CreateDnsClient(), net::BoundNetLog());
    log_client.QueryLeafIndex(log_domain, leaf_hash, callback->AsCallback());
    callback->WaitUntilRun();
  }

  void QueryAuditProof(base::StringPiece log_domain,
                       uint64_t leaf_index,
                       uint64_t tree_size,
                       MockAuditProofCallback* callback) {
    LogDnsClient log_client(mock_dns_.CreateDnsClient(), net::BoundNetLog());
    log_client.QueryAuditProof(log_domain, leaf_index, tree_size,
                               callback->AsCallback());
    callback->WaitUntilRun();
  }

  // This will be the NetworkChangeNotifier singleton for the duration of the
  // test. It is accessed statically by LogDnsClient.
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  // Queues and handles asynchronous DNS tasks. Indirectly used by LogDnsClient,
  // the underlying net::DnsClient, and NetworkChangeNotifier.
  base::MessageLoopForIO message_loop_;
  // Allows mock DNS sockets to be setup.
  MockLogDnsTraffic mock_dns_;
};

TEST_P(LogDnsClientTest, QueryLeafIndex) {
  mock_dns_.ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      "123456");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsOk());
  EXPECT_THAT(callback.leaf_index(), 123456);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsThatLogDomainDoesNotExist) {
  mock_dns_.ExpectRequestAndErrorResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::dns_protocol::kRcodeNXDOMAIN);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsServerFailure) {
  mock_dns_.ExpectRequestAndErrorResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::dns_protocol::kRcodeSERVFAIL);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsServerRefusal) {
  mock_dns_.ExpectRequestAndErrorResponse(
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
  mock_dns_.ExpectLeafIndexRequestAndResponse(
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
  mock_dns_.ExpectLeafIndexRequestAndResponse(
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
  mock_dns_.ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.", "");

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexHasNonNumericPrefix) {
  mock_dns_.ExpectLeafIndexRequestAndResponse(
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
  mock_dns_.ExpectLeafIndexRequestAndResponse(
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
  mock_dns_.ExpectRequestAndSocketError(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      net::ERR_CONNECTION_REFUSED);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHash, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_CONNECTION_REFUSED));
  EXPECT_THAT(callback.leaf_index(), 0);
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsTimeout) {
  mock_dns_.ExpectRequestAndTimeout(
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
  mock_dns_.ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                               audit_proof.begin(),
                                               audit_proof.begin() + 7);
  mock_dns_.ExpectAuditProofRequestAndResponse("7.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 7,
                                               audit_proof.begin() + 14);
  mock_dns_.ExpectAuditProofRequestAndResponse("14.123456.999999.tree.ct.test.",
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
  mock_dns_.ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                               audit_proof.begin(),
                                               audit_proof.begin() + 1);
  mock_dns_.ExpectAuditProofRequestAndResponse("1.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 1,
                                               audit_proof.begin() + 3);
  mock_dns_.ExpectAuditProofRequestAndResponse("3.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 3,
                                               audit_proof.begin() + 6);
  mock_dns_.ExpectAuditProofRequestAndResponse("6.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 6,
                                               audit_proof.begin() + 10);
  mock_dns_.ExpectAuditProofRequestAndResponse("10.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 10,
                                               audit_proof.begin() + 13);
  mock_dns_.ExpectAuditProofRequestAndResponse("13.123456.999999.tree.ct.test.",
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
  mock_dns_.ExpectRequestAndErrorResponse("0.123456.999999.tree.ct.test.",
                                          net::dns_protocol::kRcodeNXDOMAIN);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsServerFailure) {
  mock_dns_.ExpectRequestAndErrorResponse("0.123456.999999.tree.ct.test.",
                                          net::dns_protocol::kRcodeSERVFAIL);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsServerRefusal) {
  mock_dns_.ExpectRequestAndErrorResponse("0.123456.999999.tree.ct.test.",
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

  mock_dns_.ExpectAuditProofRequestAndResponse(
      "0.123456.999999.tree.ct.test.", audit_proof.begin(), audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsResponseMalformedIfNodeTooLong) {
  // node is longer than a SHA-256 hash (33 vs 32 bytes)
  const std::vector<std::string> audit_proof(1, std::string(33, 'a'));

  mock_dns_.ExpectAuditProofRequestAndResponse(
      "0.123456.999999.tree.ct.test.", audit_proof.begin(), audit_proof.end());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsResponseMalformedIfEmpty) {
  const std::vector<std::string> audit_proof;

  mock_dns_.ExpectAuditProofRequestAndResponse(
      "0.123456.999999.tree.ct.test.", audit_proof.begin(), audit_proof.end());

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
  mock_dns_.ExpectRequestAndSocketError("0.123456.999999.tree.ct.test.",
                                        net::ERR_CONNECTION_REFUSED);

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_CONNECTION_REFUSED));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, QueryAuditProofReportsTimeout) {
  mock_dns_.ExpectRequestAndTimeout("0.123456.999999.tree.ct.test.");

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_TIMED_OUT));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest, AdoptsLatestDnsConfigIfValid) {
  std::unique_ptr<net::DnsClient> tmp = mock_dns_.CreateDnsClient();
  net::DnsClient* dns_client = tmp.get();
  LogDnsClient log_client(std::move(tmp), net::BoundNetLog());

  // Get the current DNS config, modify it and broadcast the update.
  net::DnsConfig config(*dns_client->GetConfig());
  ASSERT_NE(123, config.attempts);
  config.attempts = 123;
  mock_dns_.SetDnsConfig(config);

  // Let the DNS config change propogate.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(123, dns_client->GetConfig()->attempts);
}

TEST_P(LogDnsClientTest, IgnoresLatestDnsConfigIfInvalid) {
  std::unique_ptr<net::DnsClient> tmp = mock_dns_.CreateDnsClient();
  net::DnsClient* dns_client = tmp.get();
  LogDnsClient log_client(std::move(tmp), net::BoundNetLog());

  // Get the current DNS config, modify it and broadcast the update.
  net::DnsConfig config(*dns_client->GetConfig());
  ASSERT_THAT(config.nameservers, Not(IsEmpty()));
  config.nameservers.clear();  // Makes config invalid
  mock_dns_.SetDnsConfig(config);

  // Let the DNS config change propogate.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(dns_client->GetConfig()->nameservers, Not(IsEmpty()));
}

INSTANTIATE_TEST_CASE_P(ReadMode,
                        LogDnsClientTest,
                        ::testing::Values(net::IoMode::ASYNC,
                                          net::IoMode::SYNCHRONOUS));

}  // namespace
}  // namespace certificate_transparency
