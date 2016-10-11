// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_dns_client.h"

#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/certificate_transparency/mock_log_dns_traffic.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Le;
using ::testing::Not;
using ::testing::NotNull;
using net::test::IsError;
using net::test::IsOk;

const char* const kLeafHashes[] = {
    "\x1f\x25\xe1\xca\xba\x4f\xf9\xb8\x27\x24\x83\x0f\xca\x60\xe4\xc2\xbe\xa8"
    "\xc3\xa9\x44\x1c\x27\xb0\xb4\x3e\x6a\x96\x94\xc7\xb8\x04",
    "\x2c\x26\xb4\x6b\x68\xff\xc6\x8f\xf9\x9b\x45\x3c\x1d\x30\x41\x34\x13\x42"
    "\x2d\x70\x64\x83\xbf\xa0\xf9\x8a\x5e\x88\x62\x66\xe7\xae",
    "\xfc\xde\x2b\x2e\xdb\xa5\x6b\xf4\x08\x60\x1f\xb7\x21\xfe\x9b\x5c\x33\x8d"
    "\x10\xee\x42\x9e\xa0\x4f\xae\x55\x11\xb6\x8f\xbf\x8f\xb9",
};

// Assumes log domain is "ct.test"
const char* const kBase32LeafHashes[] = {
    "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
    "FQTLI23I77DI76M3IU6B2MCBGQJUELLQMSB37IHZRJPIQYTG46XA.hash.ct.test.",
    "7TPCWLW3UVV7ICDAD63SD7U3LQZY2EHOIKPKAT5OKUI3ND57R64Q.hash.ct.test.",
};

// Leaf indices and tree sizes for use with above leaf hashes.
const uint64_t kLeafIndices[] = {0, 1, 2};
const uint64_t kTreeSizes[] = {100, 10000, 1000000};

// Only 7 audit proof nodes can fit into a DNS response, because they are sent
// in a TXT RDATA string, which has a maximum size of 255 bytes, and each node
// is a SHA-256 hash (32 bytes), i.e. (255 / 32) == 7.
// This means audit proofs consisting of more than 7 nodes require multiple DNS
// requests to retrieve.
const size_t kMaxProofNodesPerDnsResponse = 7;

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
    EXPECT_FALSE(called_);
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
    EXPECT_FALSE(called_);
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

  std::unique_ptr<LogDnsClient> CreateLogDnsClient(
      size_t max_concurrent_queries) {
    return base::MakeUnique<LogDnsClient>(mock_dns_.CreateDnsClient(),
                                          net::NetLogWithSource(),
                                          max_concurrent_queries);
  }

  void QueryLeafIndexAsync(LogDnsClient* log_client,
                           base::StringPiece log_domain,
                           const char leaf_hash[crypto::kSHA256Length],
                           const LogDnsClient::LeafIndexCallback& callback) {
    log_client->QueryLeafIndex(log_domain, leaf_hash, callback);
  }

  // Convenience function for calling QueryLeafIndexAsync synchronously.
  void QueryLeafIndex(base::StringPiece log_domain,
                      const char leaf_hash[crypto::kSHA256Length],
                      MockLeafIndexCallback* callback) {
    std::unique_ptr<LogDnsClient> log_client = CreateLogDnsClient(0);
    QueryLeafIndexAsync(log_client.get(), log_domain, leaf_hash,
                        callback->AsCallback());
    callback->WaitUntilRun();
  }

  void QueryAuditProofAsync(LogDnsClient* log_client,
                            base::StringPiece log_domain,
                            uint64_t leaf_index,
                            uint64_t tree_size,
                            const LogDnsClient::AuditProofCallback& callback) {
    log_client->QueryAuditProof(log_domain, leaf_index, tree_size, callback);
  }

  // Convenience function for calling QueryAuditProofAsync synchronously.
  void QueryAuditProof(base::StringPiece log_domain,
                       uint64_t leaf_index,
                       uint64_t tree_size,
                       MockAuditProofCallback* callback) {
    std::unique_ptr<LogDnsClient> log_client = CreateLogDnsClient(0);
    QueryAuditProofAsync(log_client.get(), log_domain, leaf_index, tree_size,
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
  mock_dns_.ExpectLeafIndexRequestAndResponse(kBase32LeafHashes[0], 123456);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsOk());
  EXPECT_THAT(callback.leaf_index(), Eq(123456u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsThatLogDomainDoesNotExist) {
  mock_dns_.ExpectRequestAndErrorResponse(kBase32LeafHashes[0],
                                          net::dns_protocol::kRcodeNXDOMAIN);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_NAME_NOT_RESOLVED));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsServerFailure) {
  mock_dns_.ExpectRequestAndErrorResponse(kBase32LeafHashes[0],
                                          net::dns_protocol::kRcodeSERVFAIL);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsServerRefusal) {
  mock_dns_.ExpectRequestAndErrorResponse(kBase32LeafHashes[0],
                                          net::dns_protocol::kRcodeREFUSED);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_SERVER_FAILED));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfContainsNoStrings) {
  mock_dns_.ExpectRequestAndResponse(
      kBase32LeafHashes[0],
      std::vector<base::StringPiece>());

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfContainsMoreThanOneString) {
  mock_dns_.ExpectRequestAndResponse(
      kBase32LeafHashes[0],
      {"123456", "7"});

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexIsNotNumeric) {
  mock_dns_.ExpectRequestAndResponse(kBase32LeafHashes[0], {"foo"});

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexIsFloatingPoint) {
  mock_dns_.ExpectRequestAndResponse(kBase32LeafHashes[0], {"123456.0"});

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexIsEmpty) {
  mock_dns_.ExpectRequestAndResponse(kBase32LeafHashes[0], {""});

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexHasNonNumericPrefix) {
  mock_dns_.ExpectRequestAndResponse(kBase32LeafHashes[0], {"foo123456"});

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest,
       QueryLeafIndexReportsMalformedResponseIfLeafIndexHasNonNumericSuffix) {
  mock_dns_.ExpectRequestAndResponse(kBase32LeafHashes[0], {"123456foo"});

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLogDomainIsEmpty) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLogDomainIsNull) {
  MockLeafIndexCallback callback;
  QueryLeafIndex(nullptr, kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLeafHashIsInvalid) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", "foo", &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLeafHashIsEmpty) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", "", &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsInvalidArgIfLeafHashIsNull) {
  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", nullptr, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_INVALID_ARGUMENT));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsSocketError) {
  mock_dns_.ExpectRequestAndSocketError(kBase32LeafHashes[0],
                                        net::ERR_CONNECTION_REFUSED);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_CONNECTION_REFUSED));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryLeafIndexReportsTimeout) {
  mock_dns_.ExpectRequestAndTimeout(kBase32LeafHashes[0]);

  MockLeafIndexCallback callback;
  QueryLeafIndex("ct.test", kLeafHashes[0], &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_TIMED_OUT));
  EXPECT_THAT(callback.leaf_index(), Eq(0u));
}

TEST_P(LogDnsClientTest, QueryAuditProof) {
  const std::vector<std::string> audit_proof = GetSampleAuditProof(20);

  // It takes a number of DNS requests to retrieve the entire |audit_proof|
  // (see |kMaxProofNodesPerDnsResponse|).
  for (size_t nodes_begin = 0; nodes_begin < audit_proof.size();
       nodes_begin += kMaxProofNodesPerDnsResponse) {
    const size_t nodes_end = std::min(
        nodes_begin + kMaxProofNodesPerDnsResponse, audit_proof.size());

    mock_dns_.ExpectAuditProofRequestAndResponse(
        base::StringPrintf("%zu.123456.999999.tree.ct.test.", nodes_begin),
        audit_proof.begin() + nodes_begin, audit_proof.begin() + nodes_end);
  }

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsOk());
  ASSERT_THAT(callback.proof(), NotNull());
  EXPECT_THAT(callback.proof()->leaf_index, Eq(123456u));
  // TODO(robpercival): Enable this once MerkleAuditProof has tree_size.
  // EXPECT_THAT(callback.proof()->tree_size, Eq(999999));
  EXPECT_THAT(callback.proof()->nodes, Eq(audit_proof));
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
  EXPECT_THAT(callback.proof()->leaf_index, Eq(123456u));
  // TODO(robpercival): Enable this once MerkleAuditProof has tree_size.
  // EXPECT_THAT(callback.proof()->tree_size, Eq(999999));
  EXPECT_THAT(callback.proof()->nodes, Eq(audit_proof));
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
       QueryAuditProofReportsResponseMalformedIfContainsNoStrings) {
  mock_dns_.ExpectRequestAndResponse("0.123456.999999.tree.ct.test.",
                                     std::vector<base::StringPiece>());

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
  EXPECT_THAT(callback.proof(), IsNull());
}

TEST_P(LogDnsClientTest,
       QueryAuditProofReportsResponseMalformedIfContainsMoreThanOneString) {
  // The CT-over-DNS draft RFC states that the response will contain "exactly
  // one character-string."
  const std::vector<std::string> audit_proof = GetSampleAuditProof(10);

  std::string first_chunk_of_proof = std::accumulate(
      audit_proof.begin(), audit_proof.begin() + 7, std::string());
  std::string second_chunk_of_proof = std::accumulate(
      audit_proof.begin() + 7, audit_proof.end(), std::string());

  mock_dns_.ExpectRequestAndResponse(
      "0.123456.999999.tree.ct.test.",
      {first_chunk_of_proof, second_chunk_of_proof});

  MockAuditProofCallback callback;
  QueryAuditProof("ct.test", 123456, 999999, &callback);
  ASSERT_TRUE(callback.called());
  EXPECT_THAT(callback.net_error(), IsError(net::ERR_DNS_MALFORMED_RESPONSE));
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
  LogDnsClient log_client(std::move(tmp), net::NetLogWithSource(), 0);

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
  LogDnsClient log_client(std::move(tmp), net::NetLogWithSource(), 0);

  // Get the current DNS config, modify it and broadcast the update.
  net::DnsConfig config(*dns_client->GetConfig());
  ASSERT_THAT(config.nameservers, Not(IsEmpty()));
  config.nameservers.clear();  // Makes config invalid
  mock_dns_.SetDnsConfig(config);

  // Let the DNS config change propogate.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(dns_client->GetConfig()->nameservers, Not(IsEmpty()));
}

TEST_P(LogDnsClientTest, CanPerformLeafIndexQueriesInParallel) {
  // Test that leaf index queries can be performed in parallel.
  constexpr size_t kNumOfParallelQueries = 3;
  ASSERT_THAT(kNumOfParallelQueries, AllOf(Le(arraysize(kLeafHashes)),
                                           Le(arraysize(kBase32LeafHashes))))
      << "Not enough test data for this many parallel queries";

  std::unique_ptr<LogDnsClient> log_client =
      CreateLogDnsClient(kNumOfParallelQueries);
  MockLeafIndexCallback callbacks[kNumOfParallelQueries];

  // Expect multiple queries.
  for (size_t i = 0; i < kNumOfParallelQueries; ++i) {
    mock_dns_.ExpectLeafIndexRequestAndResponse(
        kBase32LeafHashes[i], kLeafIndices[i]);
  }

  // Start the queries.
  for (size_t i = 0; i < kNumOfParallelQueries; ++i) {
    QueryLeafIndexAsync(log_client.get(), "ct.test", kLeafHashes[i],
                        callbacks[i].AsCallback());
  }

  // Wait for each query to complete and check its results.
  for (size_t i = 0; i < kNumOfParallelQueries; ++i) {
    MockLeafIndexCallback& callback = callbacks[i];
    callback.WaitUntilRun();

    SCOPED_TRACE(testing::Message() << "callbacks[" << i << "]");
    ASSERT_TRUE(callback.called());
    EXPECT_THAT(callback.net_error(), IsOk());
    EXPECT_THAT(callback.leaf_index(), Eq(kLeafIndices[i]));
  }
}

TEST_P(LogDnsClientTest, CanPerformAuditProofQueriesInParallel) {
  // Check that 3 audit proof queries can be performed in parallel.
  constexpr size_t kNumOfParallelQueries = 3;
  ASSERT_THAT(kNumOfParallelQueries,
              AllOf(Le(arraysize(kLeafIndices)), Le(arraysize(kTreeSizes))))
      << "Not enough test data for this many parallel queries";

  std::unique_ptr<LogDnsClient> log_client =
      CreateLogDnsClient(kNumOfParallelQueries);
  MockAuditProofCallback callbacks[kNumOfParallelQueries];

  // Each query should require one more DNS request than the last.
  // This helps to test that parallel queries do not intefere with each other,
  // e.g. one query causing another to end prematurely.
  std::vector<std::string> audit_proofs[kNumOfParallelQueries];
  for (size_t query_i = 0; query_i < kNumOfParallelQueries; ++query_i) {
    const size_t dns_requests_required = query_i + 1;
    audit_proofs[query_i] = GetSampleAuditProof(dns_requests_required *
                                                kMaxProofNodesPerDnsResponse);
  }
  // The most DNS requests that are made by any of the above N queries is N.
  const size_t kMaxDnsRequestsPerQuery = kNumOfParallelQueries;

  // Setup expectations for up to N DNS requests per query performed.
  // All of the queries will be started at the same time, so expect the DNS
  // requests and responses to be interleaved.
  // NB:
  // Ideally, the tests wouldn't require that the DNS requests sent by the
  // parallel queries are interleaved. However, the mock socket framework does
  // not provide a way to express this.
  for (size_t dns_req_i = 0; dns_req_i < kMaxDnsRequestsPerQuery; ++dns_req_i) {
    for (size_t query_i = 0; query_i < kNumOfParallelQueries; ++query_i) {
      const std::vector<std::string>& proof = audit_proofs[query_i];
      // Closed-open range of |proof| nodes that are expected in this response.
      const size_t start_node = dns_req_i * 7;
      const size_t end_node =
          std::min(start_node + kMaxProofNodesPerDnsResponse, proof.size());

      // If there are any nodes left, expect another request and response.
      if (start_node < end_node) {
        mock_dns_.ExpectAuditProofRequestAndResponse(
            base::StringPrintf("%zu.%" PRIu64 ".%" PRIu64 ".tree.ct.test.",
                               start_node, kLeafIndices[query_i],
                               kTreeSizes[query_i]),
            proof.begin() + start_node, proof.begin() + end_node);
      }
    }
  }

  // Start the queries.
  for (size_t i = 0; i < kNumOfParallelQueries; ++i) {
    QueryAuditProofAsync(log_client.get(), "ct.test", kLeafIndices[i],
                         kTreeSizes[i], callbacks[i].AsCallback());
  }

  // Wait for each query to complete and check its results.
  for (size_t i = 0; i < kNumOfParallelQueries; ++i) {
    MockAuditProofCallback& callback = callbacks[i];
    callbacks[i].WaitUntilRun();

    SCOPED_TRACE(testing::Message() << "callbacks[" << i << "]");
    ASSERT_TRUE(callback.called());
    EXPECT_THAT(callback.net_error(), IsOk());
    ASSERT_THAT(callback.proof(), NotNull());
    EXPECT_THAT(callback.proof()->leaf_index, Eq(kLeafIndices[i]));
    // TODO(robpercival): Enable this once MerkleAuditProof has tree_size.
    // EXPECT_THAT(callback.proof()->tree_size, kTreeSizes[i]);
    EXPECT_THAT(callback.proof()->nodes, Eq(audit_proofs[i]));
  }
}

TEST_P(LogDnsClientTest, CanPerformLeafIndexAndAuditProofQueriesInParallel) {
  // Check that a leaf index and audit proof query can be performed in parallel.
  constexpr size_t kNumOfParallelQueries = 2;
  std::unique_ptr<LogDnsClient> log_client =
      CreateLogDnsClient(kNumOfParallelQueries);
  MockLeafIndexCallback leaf_index_callback;
  MockAuditProofCallback audit_proof_callback;
  const std::vector<std::string> audit_proof = GetSampleAuditProof(20);

  mock_dns_.ExpectLeafIndexRequestAndResponse(kBase32LeafHashes[0], 123456);

  // It should require 3 requests to collect the entire audit proof, as there is
  // only space for 7 nodes per TXT record. One node is 32 bytes long and the
  // TXT RDATA can have a maximum length of 255 bytes (255 / 32).
  mock_dns_.ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                               audit_proof.begin(),
                                               audit_proof.begin() + 7);
  mock_dns_.ExpectAuditProofRequestAndResponse("7.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 7,
                                               audit_proof.begin() + 14);
  mock_dns_.ExpectAuditProofRequestAndResponse("14.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 14,
                                               audit_proof.end());

  // Start the queries.
  QueryLeafIndexAsync(log_client.get(), "ct.test", kLeafHashes[0],
                      leaf_index_callback.AsCallback());
  QueryAuditProofAsync(log_client.get(), "ct.test", 123456, 999999,
                       audit_proof_callback.AsCallback());

  // Wait for the queries to complete, then check their results.
  leaf_index_callback.WaitUntilRun();
  audit_proof_callback.WaitUntilRun();

  ASSERT_TRUE(leaf_index_callback.called());
  EXPECT_THAT(leaf_index_callback.net_error(), IsOk());
  EXPECT_THAT(leaf_index_callback.leaf_index(), Eq(123456u));

  ASSERT_TRUE(audit_proof_callback.called());
  EXPECT_THAT(audit_proof_callback.net_error(), IsOk());
  ASSERT_THAT(audit_proof_callback.proof(), NotNull());
  EXPECT_THAT(audit_proof_callback.proof()->leaf_index, Eq(123456u));
  // TODO(robpercival): Enable this once MerkleAuditProof has tree_size.
  // EXPECT_THAT(audit_proof_callback.proof()->tree_size, Eq(999999));
  EXPECT_THAT(audit_proof_callback.proof()->nodes, Eq(audit_proof));
}

TEST_P(LogDnsClientTest, CanBeThrottledToOneLeafIndexQueryAtATime) {
  // Check that leaf index queries can be rate-limited to one at a time.
  // The second query, initiated while the first is in progress, should fail.
  mock_dns_.ExpectLeafIndexRequestAndResponse(kBase32LeafHashes[0], 123456);

  const size_t max_concurrent_queries = 1;
  std::unique_ptr<LogDnsClient> log_client =
      CreateLogDnsClient(max_concurrent_queries);

  // Start the queries.
  MockLeafIndexCallback callback1;
  QueryLeafIndexAsync(log_client.get(), "ct.test", kLeafHashes[0],
                      callback1.AsCallback());
  MockLeafIndexCallback callback2;
  QueryLeafIndexAsync(log_client.get(), "ct.test", kLeafHashes[1],
                      callback2.AsCallback());

  callback1.WaitUntilRun();
  callback2.WaitUntilRun();

  // Check that the first query succeeded.
  ASSERT_TRUE(callback1.called());
  EXPECT_THAT(callback1.net_error(), IsOk());
  EXPECT_THAT(callback1.leaf_index(), Eq(123456u));

  // Check that the second query failed.
  ASSERT_TRUE(callback2.called());
  EXPECT_THAT(callback2.net_error(), IsError(net::ERR_TEMPORARILY_THROTTLED));
  EXPECT_THAT(callback2.leaf_index(), Eq(0u));

  // Try a third query, which should succeed now that the first is finished.
  mock_dns_.ExpectLeafIndexRequestAndResponse(kBase32LeafHashes[2], 666);

  MockLeafIndexCallback callback3;
  QueryLeafIndexAsync(log_client.get(), "ct.test", kLeafHashes[2],
                      callback3.AsCallback());

  callback3.WaitUntilRun();

  // Check that the third query succeeded.
  ASSERT_TRUE(callback3.called());
  EXPECT_THAT(callback3.net_error(), IsOk());
  EXPECT_THAT(callback3.leaf_index(), Eq(666u));
}

TEST_P(LogDnsClientTest, CanBeThrottledToOneAuditProofQueryAtATime) {
  // Check that audit proof queries can be rate-limited to one at a time.
  // The second query, initiated while the first is in progress, should fail.
  const std::vector<std::string> audit_proof = GetSampleAuditProof(20);

  // It should require 3 requests to collect the entire audit proof, as there is
  // only space for 7 nodes per TXT record. One node is 32 bytes long and the
  // TXT RDATA can have a maximum length of 255 bytes (255 / 32).
  // Rate limiting should not interfere with these requests.
  mock_dns_.ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                               audit_proof.begin(),
                                               audit_proof.begin() + 7);
  mock_dns_.ExpectAuditProofRequestAndResponse("7.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 7,
                                               audit_proof.begin() + 14);
  mock_dns_.ExpectAuditProofRequestAndResponse("14.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 14,
                                               audit_proof.end());

  const size_t max_concurrent_queries = 1;
  std::unique_ptr<LogDnsClient> log_client =
      CreateLogDnsClient(max_concurrent_queries);

  // Start the queries.
  MockAuditProofCallback callback1;
  QueryAuditProofAsync(log_client.get(), "ct.test", 123456, 999999,
                       callback1.AsCallback());
  MockAuditProofCallback callback2;
  QueryAuditProofAsync(log_client.get(), "ct.test", 123456, 999999,
                       callback2.AsCallback());

  callback1.WaitUntilRun();
  callback2.WaitUntilRun();

  // Check that the first query succeeded.
  ASSERT_TRUE(callback1.called());
  EXPECT_THAT(callback1.net_error(), IsOk());
  ASSERT_THAT(callback1.proof(), NotNull());
  EXPECT_THAT(callback1.proof()->leaf_index, Eq(123456u));
  // TODO(robpercival): Enable this once MerkleAuditProof has tree_size.
  // EXPECT_THAT(callback1.proof()->tree_size, Eq(999999));
  EXPECT_THAT(callback1.proof()->nodes, Eq(audit_proof));

  // Check that the second query failed.
  ASSERT_TRUE(callback2.called());
  EXPECT_THAT(callback2.net_error(), IsError(net::ERR_TEMPORARILY_THROTTLED));
  EXPECT_THAT(callback2.proof(), IsNull());

  // Try a third query, which should succeed now that the first is finished.
  mock_dns_.ExpectAuditProofRequestAndResponse("0.123456.999999.tree.ct.test.",
                                               audit_proof.begin(),
                                               audit_proof.begin() + 7);
  mock_dns_.ExpectAuditProofRequestAndResponse("7.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 7,
                                               audit_proof.begin() + 14);
  mock_dns_.ExpectAuditProofRequestAndResponse("14.123456.999999.tree.ct.test.",
                                               audit_proof.begin() + 14,
                                               audit_proof.end());

  MockAuditProofCallback callback3;
  QueryAuditProofAsync(log_client.get(), "ct.test", 123456, 999999,
                       callback3.AsCallback());

  callback3.WaitUntilRun();

  // Check that the third query succeeded.
  ASSERT_TRUE(callback3.called());
  EXPECT_THAT(callback3.net_error(), IsOk());
  ASSERT_THAT(callback3.proof(), NotNull());
  EXPECT_THAT(callback3.proof()->leaf_index, Eq(123456u));
  // TODO(robpercival): Enable this once MerkleAuditProof has tree_size.
  // EXPECT_THAT(callback3.proof()->tree_size, Eq(999999));
  EXPECT_THAT(callback3.proof()->nodes, Eq(audit_proof));
}

TEST_P(LogDnsClientTest, ThrottlingAppliesAcrossQueryTypes) {
  // Check that queries can be rate-limited to one at a time, regardless of the
  // type of query. The second query, initiated while the first is in progress,
  // should fail.
  mock_dns_.ExpectLeafIndexRequestAndResponse(
      "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
      123456);

  const size_t max_concurrent_queries = 1;
  std::unique_ptr<LogDnsClient> log_client =
      CreateLogDnsClient(max_concurrent_queries);

  // Start the queries.
  MockLeafIndexCallback leaf_index_callback;
  QueryLeafIndexAsync(log_client.get(), "ct.test", kLeafHashes[0],
                      leaf_index_callback.AsCallback());
  MockAuditProofCallback audit_proof_callback;
  QueryAuditProofAsync(log_client.get(), "ct.test", 123456, 999999,
                       audit_proof_callback.AsCallback());

  leaf_index_callback.WaitUntilRun();
  audit_proof_callback.WaitUntilRun();

  // Check that the first query succeeded.
  ASSERT_TRUE(leaf_index_callback.called());
  EXPECT_THAT(leaf_index_callback.net_error(), IsOk());
  EXPECT_THAT(leaf_index_callback.leaf_index(), Eq(123456u));

  // Check that the second query failed.
  ASSERT_TRUE(audit_proof_callback.called());
  EXPECT_THAT(audit_proof_callback.net_error(),
              IsError(net::ERR_TEMPORARILY_THROTTLED));
  EXPECT_THAT(audit_proof_callback.proof(), IsNull());
}

INSTANTIATE_TEST_CASE_P(ReadMode,
                        LogDnsClientTest,
                        ::testing::Values(net::IoMode::ASYNC,
                                          net::IoMode::SYNCHRONOUS));

}  // namespace
}  // namespace certificate_transparency
