// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_dns_client.h"

#include <sstream>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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

  *txt = base::JoinString(txt_record->texts(), "");
  return true;
}

bool ParseLeafIndex(const net::DnsResponse& response, uint64_t* index) {
  DCHECK(index);

  std::string index_str;
  if (!ParseTxtResponse(response, &index_str))
    return false;

  return base::StringToUint64(index_str, index);
}

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

LogDnsClient::LogDnsClient(std::unique_ptr<net::DnsClient> dns_client,
                           const net::BoundNetLog& net_log)
    : dns_client_(std::move(dns_client)),
      net_log_(net_log),
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

void LogDnsClient::QueryLeafIndex(base::StringPiece domain_for_log,
                                  base::StringPiece leaf_hash,
                                  const LeafIndexCallback& callback) {
  if (domain_for_log.empty() || leaf_hash.size() != crypto::kSHA256Length) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, net::Error::ERR_INVALID_ARGUMENT, 0));
    return;
  }

  std::string encoded_leaf_hash =
      base32::Base32Encode(leaf_hash, base32::Base32EncodePolicy::OMIT_PADDING);
  DCHECK_EQ(encoded_leaf_hash.size(), 52u);

  net::DnsTransactionFactory* factory = dns_client_->GetTransactionFactory();
  if (factory == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, net::Error::ERR_NAME_RESOLUTION_FAILED, 0));
    return;
  }

  std::ostringstream qname;
  qname << encoded_leaf_hash << ".hash." << domain_for_log << ".";

  net::DnsTransactionFactory::CallbackType transaction_callback = base::Bind(
      &LogDnsClient::QueryLeafIndexComplete, weak_ptr_factory_.GetWeakPtr());

  std::unique_ptr<net::DnsTransaction> dns_transaction =
      factory->CreateTransaction(qname.str(), net::dns_protocol::kTypeTXT,
                                 transaction_callback, net_log_);

  dns_transaction->Start();
  leaf_index_queries_.push_back({std::move(dns_transaction), callback});
}

// The performance of this could be improved by sending all of the expected
// queries up front. Each response can contain a maximum of 7 audit path nodes,
// so for an audit proof of size 20, it could send 3 queries (for nodes 0-6,
// 7-13 and 14-19) immediately. Currently, it sends only the first and then,
// based on the number of nodes received, sends the next query. The complexity
// of the code would increase though, as it would need to detect gaps in the
// audit proof caused by the server not responding with the anticipated number
// of nodes. Ownership of the proof would need to change, as it would be shared
// between simultaneous DNS transactions.
void LogDnsClient::QueryAuditProof(base::StringPiece domain_for_log,
                                   uint64_t leaf_index,
                                   uint64_t tree_size,
                                   const AuditProofCallback& callback) {
  if (domain_for_log.empty() || leaf_index >= tree_size) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, net::Error::ERR_INVALID_ARGUMENT, nullptr));
    return;
  }

  std::unique_ptr<net::ct::MerkleAuditProof> proof(
      new net::ct::MerkleAuditProof);
  proof->leaf_index = leaf_index;
  // TODO(robpercival): Once a "tree_size" field is added to MerkleAuditProof,
  // pass |tree_size| to QueryAuditProofNodes using that.

  // Query for the first batch of audit proof nodes (i.e. starting from 0).
  QueryAuditProofNodes(std::move(proof), domain_for_log, tree_size, 0,
                       callback);
}

void LogDnsClient::QueryLeafIndexComplete(net::DnsTransaction* transaction,
                                          int net_error,
                                          const net::DnsResponse* response) {
  auto query_iterator =
      std::find_if(leaf_index_queries_.begin(), leaf_index_queries_.end(),
                   [transaction](const Query<LeafIndexCallback>& query) {
                     return query.transaction.get() == transaction;
                   });
  if (query_iterator == leaf_index_queries_.end()) {
    NOTREACHED();
    return;
  }
  const Query<LeafIndexCallback> query = std::move(*query_iterator);
  leaf_index_queries_.erase(query_iterator);

  // If we've received no response but no net::error either (shouldn't happen),
  // report the response as invalid.
  if (response == nullptr && net_error == net::OK) {
    net_error = net::ERR_INVALID_RESPONSE;
  }

  if (net_error != net::OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(query.callback, net_error, 0));
    return;
  }

  uint64_t leaf_index;
  if (!ParseLeafIndex(*response, &leaf_index)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(query.callback, net::ERR_DNS_MALFORMED_RESPONSE, 0));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(query.callback, net::OK, leaf_index));
}

void LogDnsClient::QueryAuditProofNodes(
    std::unique_ptr<net::ct::MerkleAuditProof> proof,
    base::StringPiece domain_for_log,
    uint64_t tree_size,
    uint64_t node_index,
    const AuditProofCallback& callback) {
  // Preconditions that should be guaranteed internally by this class.
  DCHECK(proof);
  DCHECK(!domain_for_log.empty());
  DCHECK_LT(proof->leaf_index, tree_size);
  DCHECK_LT(node_index,
            net::ct::CalculateAuditPathLength(proof->leaf_index, tree_size));

  net::DnsTransactionFactory* factory = dns_client_->GetTransactionFactory();
  if (factory == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, net::Error::ERR_NAME_RESOLUTION_FAILED, nullptr));
    return;
  }

  std::ostringstream qname;
  qname << node_index << "." << proof->leaf_index << "." << tree_size
        << ".tree." << domain_for_log << ".";

  net::DnsTransactionFactory::CallbackType transaction_callback =
      base::Bind(&LogDnsClient::QueryAuditProofNodesComplete,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(std::move(proof)),
                 domain_for_log, tree_size);

  std::unique_ptr<net::DnsTransaction> dns_transaction =
      factory->CreateTransaction(qname.str(), net::dns_protocol::kTypeTXT,
                                 transaction_callback, net_log_);
  dns_transaction->Start();
  audit_proof_queries_.push_back({std::move(dns_transaction), callback});
}

void LogDnsClient::QueryAuditProofNodesComplete(
    std::unique_ptr<net::ct::MerkleAuditProof> proof,
    base::StringPiece domain_for_log,
    uint64_t tree_size,
    net::DnsTransaction* transaction,
    int net_error,
    const net::DnsResponse* response) {
  // Preconditions that should be guaranteed internally by this class.
  DCHECK(proof);
  DCHECK(!domain_for_log.empty());

  auto query_iterator =
      std::find_if(audit_proof_queries_.begin(), audit_proof_queries_.end(),
                   [transaction](const Query<AuditProofCallback>& query) {
                     return query.transaction.get() == transaction;
                   });

  if (query_iterator == audit_proof_queries_.end()) {
    NOTREACHED();
    return;
  }
  const Query<AuditProofCallback> query = std::move(*query_iterator);
  audit_proof_queries_.erase(query_iterator);

  // If we've received no response but no net::error either (shouldn't happen),
  // report the response as invalid.
  if (response == nullptr && net_error == net::OK) {
    net_error = net::ERR_INVALID_RESPONSE;
  }

  if (net_error != net::OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(query.callback, net_error, nullptr));
    return;
  }

  const uint64_t audit_path_length =
      net::ct::CalculateAuditPathLength(proof->leaf_index, tree_size);
  proof->nodes.reserve(audit_path_length);

  if (!ParseAuditPath(*response, proof.get())) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(query.callback, net::ERR_DNS_MALFORMED_RESPONSE, nullptr));
    return;
  }

  const uint64_t audit_path_nodes_received = proof->nodes.size();
  if (audit_path_nodes_received < audit_path_length) {
    QueryAuditProofNodes(std::move(proof), domain_for_log, tree_size,
                         audit_path_nodes_received, query.callback);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(query.callback, net::OK, base::Passed(std::move(proof))));
}

void LogDnsClient::UpdateDnsConfig() {
  net::DnsConfig config;
  net::NetworkChangeNotifier::GetDnsConfig(&config);
  if (config.IsValid())
    dns_client_->SetConfig(config);
}

}  // namespace certificate_transparency
