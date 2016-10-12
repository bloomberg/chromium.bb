// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_dns_client.h"

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/base32/base32.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace certificate_transparency {

namespace {

// Parses the DNS response and extracts a single string from the TXT RDATA.
// If the response is malformed, not a TXT record, or contains any number of
// strings other than 1, this returns false and extracts nothing.
// Otherwise, it returns true and the extracted string is assigned to |*txt|.
bool ParseTxtResponse(const net::DnsResponse& response, std::string* txt) {
  DCHECK(txt);

  net::DnsRecordParser parser = response.Parser();
  // We don't care about the creation time, since we're going to throw
  // |parsed_record| away as soon as we've extracted the payload, so provide
  // the "null" time.
  auto parsed_record = net::RecordParsed::CreateFrom(&parser, base::Time());
  if (parsed_record == nullptr)
    return false;

  auto* txt_record = parsed_record->rdata<net::TxtRecordRdata>();
  if (txt_record == nullptr)
    return false;

  // The draft CT-over-DNS RFC says that there MUST be exactly one string in the
  // TXT record.
  if (txt_record->texts().size() != 1)
    return false;

  *txt = txt_record->texts().front();
  return true;
}

// Extracts a leaf index value from a DNS response's TXT RDATA.
// Returns true on success, false otherwise.
bool ParseLeafIndex(const net::DnsResponse& response, uint64_t* index) {
  DCHECK(index);

  std::string index_str;
  if (!ParseTxtResponse(response, &index_str))
    return false;

  return base::StringToUint64(index_str, index);
}

// Extracts audit proof nodes from a DNS response's TXT RDATA.
// Returns true on success, false otherwise.
// It will fail if there is not a whole number of nodes present > 0.
// There must only be one string in the TXT RDATA.
// The nodes will be appended to |proof->nodes|
bool ParseAuditPath(const net::DnsResponse& response,
                    net::ct::MerkleAuditProof* proof) {
  DCHECK(proof);

  std::string audit_path;
  if (!ParseTxtResponse(response, &audit_path))
    return false;
  // If empty or not a multiple of the node size, it is considered invalid.
  // It's important to consider empty audit paths as invalid, as otherwise an
  // infinite loop could occur if the server consistently returned empty
  // responses.
  if (audit_path.empty() || audit_path.size() % crypto::kSHA256Length != 0)
    return false;

  for (size_t i = 0; i < audit_path.size(); i += crypto::kSHA256Length) {
    proof->nodes.push_back(audit_path.substr(i, crypto::kSHA256Length));
  }

  return true;
}

}  // namespace

// Encapsulates the state machine required to get an audit proof from a Merkle
// leaf hash. This requires a DNS request to obtain the leaf index, then a
// series of DNS requests to get the nodes of the proof.
class LogDnsClient::AuditProofQuery {
 public:
  using CompletionCallback =
      base::Callback<void(int net_error, AuditProofQuery* query)>;

  // The LogDnsClient is guaranteed to outlive the AuditProofQuery, so it's safe
  // to leave ownership of |dns_client| with LogDnsClient.
  AuditProofQuery(net::DnsClient* dns_client,
                  const std::string& domain_for_log,
                  uint64_t tree_size,
                  const net::NetLogWithSource& net_log);

  // Begins the process of getting an audit proof for the CT log entry with a
  // leaf hash of |leaf_hash|. The |callback| will be invoked when finished.
  void Start(base::StringPiece leaf_hash, CompletionCallback callback);

  // Transfers the audit proof to the caller.
  // Only call this once the query has completed, otherwise the proof will be
  // incomplete.
  std::unique_ptr<net::ct::MerkleAuditProof> TakeProof();

 private:
  // Requests the leaf index of the CT log entry with |leaf_hash|.
  void QueryLeafIndex(base::StringPiece leaf_hash);

  // Processes the response to a leaf index request.
  // The received leaf index will be added to the proof.
  void QueryLeafIndexComplete(net::DnsTransaction* transaction,
                              int net_error,
                              const net::DnsResponse* response);

  // Queries a CT log to retrieve part of an audit proof. The |node_index|
  // indicates which node of the audit proof/ should be requested. The CT log
  // may return up to 7 nodes, starting from |node_index| (this is the maximum
  // that will fit in a DNS UDP packet). The nodes will be appended to the
  // proof.
  void QueryAuditProofNodes(uint64_t node_index);

  // Processes the response to an audit proof request.
  // This will contain some, but not necessarily all, of the audit proof nodes.
  void QueryAuditProofNodesComplete(net::DnsTransaction* transaction,
                                    int net_error,
                                    const net::DnsResponse* response);

  std::string domain_for_log_;
  // TODO(robpercival): Remove |tree_size| once |proof_| has a tree_size member.
  uint64_t tree_size_;
  std::unique_ptr<net::ct::MerkleAuditProof> proof_;
  CompletionCallback callback_;
  net::DnsClient* dns_client_;
  std::unique_ptr<net::DnsTransaction> current_dns_transaction_;
  net::NetLogWithSource net_log_;
  base::WeakPtrFactory<AuditProofQuery> weak_ptr_factory_;
};

LogDnsClient::AuditProofQuery::AuditProofQuery(
    net::DnsClient* dns_client,
    const std::string& domain_for_log,
    uint64_t tree_size,
    const net::NetLogWithSource& net_log)
    : domain_for_log_(domain_for_log),
      tree_size_(tree_size),
      dns_client_(dns_client),
      net_log_(net_log),
      weak_ptr_factory_(this) {
  DCHECK(dns_client_);
  DCHECK(!domain_for_log_.empty());
}

void LogDnsClient::AuditProofQuery::Start(base::StringPiece leaf_hash,
                                          CompletionCallback callback) {
  current_dns_transaction_.reset();
  proof_ = base::MakeUnique<net::ct::MerkleAuditProof>();
  callback_ = callback;
  QueryLeafIndex(leaf_hash);
}

std::unique_ptr<net::ct::MerkleAuditProof>
LogDnsClient::AuditProofQuery::TakeProof() {
  return std::move(proof_);
}

void LogDnsClient::AuditProofQuery::QueryLeafIndex(
    base::StringPiece leaf_hash) {
  std::string encoded_leaf_hash =
      base32::Base32Encode(leaf_hash, base32::Base32EncodePolicy::OMIT_PADDING);
  DCHECK_EQ(encoded_leaf_hash.size(), 52u);

  std::string qname = base::StringPrintf(
      "%s.hash.%s.", encoded_leaf_hash.c_str(), domain_for_log_.data());

  net::DnsTransactionFactory* factory = dns_client_->GetTransactionFactory();
  if (factory == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net::Error::ERR_NAME_RESOLUTION_FAILED,
                              base::Unretained(this)));
    return;
  }

  net::DnsTransactionFactory::CallbackType transaction_callback =
      base::Bind(&LogDnsClient::AuditProofQuery::QueryLeafIndexComplete,
                 weak_ptr_factory_.GetWeakPtr());

  current_dns_transaction_ = factory->CreateTransaction(
      qname, net::dns_protocol::kTypeTXT, transaction_callback, net_log_);

  current_dns_transaction_->Start();
}

void LogDnsClient::AuditProofQuery::QueryLeafIndexComplete(
    net::DnsTransaction* transaction,
    int net_error,
    const net::DnsResponse* response) {
  // If we've received no response but no net::error either (shouldn't
  // happen),
  // report the response as invalid.
  if (response == nullptr && net_error == net::OK) {
    net_error = net::ERR_INVALID_RESPONSE;
  }

  if (net_error != net::OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net_error, base::Unretained(this)));
    return;
  }

  if (!ParseLeafIndex(*response, &proof_->leaf_index)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net::ERR_DNS_MALFORMED_RESPONSE,
                              base::Unretained(this)));
    return;
  }

  // Reject leaf index if it is out-of-range.
  // This indicates either:
  // a) the wrong tree_size was provided.
  // b) the wrong leaf hash was provided.
  // c) there is a bug server-side.
  // The first two are more likely, so return ERR_INVALID_ARGUMENT.
  if (proof_->leaf_index >= tree_size_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net::ERR_INVALID_ARGUMENT,
                              base::Unretained(this)));
    return;
  }

  // QueryAuditProof for the first batch of audit proof_ nodes (i.e. starting
  // from 0).
  QueryAuditProofNodes(0 /* start node index */);
}

void LogDnsClient::AuditProofQuery::QueryAuditProofNodes(uint64_t node_index) {
  DCHECK_LT(proof_->leaf_index, tree_size_);
  DCHECK_LT(node_index,
            net::ct::CalculateAuditPathLength(proof_->leaf_index, tree_size_));

  std::string qname = base::StringPrintf(
      "%" PRIu64 ".%" PRIu64 ".%" PRIu64 ".tree.%s.", node_index,
      proof_->leaf_index, tree_size_, domain_for_log_.data());

  net::DnsTransactionFactory* factory = dns_client_->GetTransactionFactory();
  if (factory == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net::Error::ERR_NAME_RESOLUTION_FAILED,
                              base::Unretained(this)));
    return;
  }

  net::DnsTransactionFactory::CallbackType transaction_callback =
      base::Bind(&LogDnsClient::AuditProofQuery::QueryAuditProofNodesComplete,
                 weak_ptr_factory_.GetWeakPtr());

  current_dns_transaction_ = factory->CreateTransaction(
      qname, net::dns_protocol::kTypeTXT, transaction_callback, net_log_);
  current_dns_transaction_->Start();
}

void LogDnsClient::AuditProofQuery::QueryAuditProofNodesComplete(
    net::DnsTransaction* transaction,
    int net_error,
    const net::DnsResponse* response) {
  // If we receive no response but no net::error either (shouldn't happen),
  // report the response as invalid.
  if (response == nullptr && net_error == net::OK) {
    net_error = net::ERR_INVALID_RESPONSE;
  }

  if (net_error != net::OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net_error, base::Unretained(this)));
    return;
  }

  const uint64_t audit_path_length =
      net::ct::CalculateAuditPathLength(proof_->leaf_index, tree_size_);
  // The calculated |audit_path_length| can't ever be greater than 64, so
  // deriving the amount of memory to reserve from the untrusted |leaf_index|
  // is safe.
  proof_->nodes.reserve(audit_path_length);

  if (!ParseAuditPath(*response, proof_.get())) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, net::ERR_DNS_MALFORMED_RESPONSE,
                              base::Unretained(this)));
    return;
  }

  const uint64_t audit_path_nodes_received = proof_->nodes.size();
  if (audit_path_nodes_received < audit_path_length) {
    QueryAuditProofNodes(audit_path_nodes_received);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback_, net::OK, base::Unretained(this)));
}

LogDnsClient::LogDnsClient(std::unique_ptr<net::DnsClient> dns_client,
                           const net::NetLogWithSource& net_log,
                           size_t max_concurrent_queries)
    : dns_client_(std::move(dns_client)),
      net_log_(net_log),
      max_concurrent_queries_(max_concurrent_queries),
      weak_ptr_factory_(this) {
  CHECK(dns_client_);
  net::NetworkChangeNotifier::AddDNSObserver(this);
  UpdateDnsConfig();
}

LogDnsClient::~LogDnsClient() {
  net::NetworkChangeNotifier::RemoveDNSObserver(this);
}

void LogDnsClient::OnDNSChanged() {
  UpdateDnsConfig();
}

void LogDnsClient::OnInitialDNSConfigRead() {
  UpdateDnsConfig();
}

// The performance of this could be improved by sending all of the expected
// queries up front. Each response can contain a maximum of 7 audit path nodes,
// so for an audit proof of size 20, it could send 3 queries (for nodes 0-6,
// 7-13 and 14-19) immediately. Currently, it sends only the first and then,
// based on the number of nodes received, sends the next query. The complexity
// of the code would increase though, as it would need to detect gaps in the
// audit proof caused by the server not responding with the anticipated number
// of nodes. Ownership of the proof would need to change, as it would be shared
// between simultaneous DNS transactions. Throttling of queries would also need
// to take into account this increase in parallelism.
void LogDnsClient::QueryAuditProof(const std::string& domain_for_log,
                                   base::StringPiece leaf_hash,
                                   uint64_t tree_size,
                                   const AuditProofCallback& callback) {
  if (domain_for_log.empty() || leaf_hash.size() != crypto::kSHA256Length) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, net::Error::ERR_INVALID_ARGUMENT, nullptr));
    return;
  }

  if (HasMaxConcurrentQueriesInProgress()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, net::Error::ERR_TEMPORARILY_THROTTLED, nullptr));
    return;
  }

  audit_proof_queries_.emplace_back(new AuditProofQuery(
      dns_client_.get(), domain_for_log, tree_size, net_log_));

  AuditProofQuery::CompletionCallback internal_callback =
      base::Bind(&LogDnsClient::QueryAuditProofComplete,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  audit_proof_queries_.back()->Start(leaf_hash, internal_callback);
}

void LogDnsClient::QueryAuditProofComplete(const AuditProofCallback& callback,
                                           int result,
                                           AuditProofQuery* query) {
  DCHECK(query);

  std::unique_ptr<net::ct::MerkleAuditProof> proof;
  if (result == net::OK) {
    proof = query->TakeProof();
  }

  // Finished with the query - destroy it.
  auto query_iterator =
      std::find_if(audit_proof_queries_.begin(), audit_proof_queries_.end(),
                   [query](const std::unique_ptr<AuditProofQuery>& p) {
                     return p.get() == query;
                   });
  DCHECK(query_iterator != audit_proof_queries_.end());
  audit_proof_queries_.erase(query_iterator);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, result, base::Passed(&proof)));
}

bool LogDnsClient::HasMaxConcurrentQueriesInProgress() const {
  return max_concurrent_queries_ != 0 &&
         audit_proof_queries_.size() >= max_concurrent_queries_;
}

void LogDnsClient::UpdateDnsConfig() {
  net::DnsConfig config;
  net::NetworkChangeNotifier::GetDnsConfig(&config);
  if (config.IsValid())
    dns_client_->SetConfig(config);
}

}  // namespace certificate_transparency
