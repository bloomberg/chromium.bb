// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_DNS_CLIENT_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_DNS_CLIENT_H_

#include <stdint.h>

#include <list>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_with_source.h"

namespace net {
class DnsClient;
class DnsResponse;
class DnsTransaction;
namespace ct {
struct MerkleAuditProof;
}  // namespace ct
}  // namespace net

namespace certificate_transparency {

// Queries Certificate Transparency (CT) log servers via DNS.
// All queries are performed asynchronously.
// For more information, see
// https://github.com/google/certificate-transparency-rfcs/blob/master/dns/draft-ct-over-dns.md.
// It must be created and deleted on the same thread. It is not thread-safe.
class LogDnsClient : public net::NetworkChangeNotifier::DNSObserver {
 public:
  // Invoked when a leaf index query completes.
  // If an error occured, |net_error| will be a net::Error code, otherwise it
  // will be net::OK and |leaf_index| will be the leaf index that was received.
  using LeafIndexCallback =
      base::Callback<void(int net_error, uint64_t leaf_index)>;
  // Invoked when an audit proof query completes.
  // If an error occurred, |net_error| will be a net::Error code, otherwise it
  // will be net::OK and |proof| will be the audit proof that was received.
  // The log ID of |proof| will not be set, as that is not known by this class,
  // but the leaf index will be set.
  using AuditProofCallback =
      base::Callback<void(int net_error,
                          std::unique_ptr<net::ct::MerkleAuditProof> proof)>;

  // Creates a log client that will take ownership of |dns_client| and use it
  // to perform DNS queries. Queries will be logged to |net_log|.
  // The |dns_client| does not need to be configured first - this will be done
  // automatically as needed.
  LogDnsClient(std::unique_ptr<net::DnsClient> dns_client,
               const net::NetLogWithSource& net_log);
  // Must be deleted on the same thread that it was created on.
  ~LogDnsClient() override;

  // Called by NetworkChangeNotifier when the DNS config changes.
  // The DnsClient's config will be updated in response.
  void OnDNSChanged() override;

  // Called by NetworkChangeNotifier when the DNS config is first read.
  // The DnsClient's config will be updated in response.
  void OnInitialDNSConfigRead() override;

  // Queries a CT log to discover the index of the leaf with |leaf_hash|.
  // The log is identified by |domain_for_log|, which is the DNS name used as a
  // suffix for all queries.
  // The |leaf_hash| is the SHA-256 hash of a Merkle tree leaf in that log.
  // The |callback| is invoked when the query is complete, or an error occurs.
  void QueryLeafIndex(base::StringPiece domain_for_log,
                      base::StringPiece leaf_hash,
                      const LeafIndexCallback& callback);

  // Queries a CT log to retrieve an audit proof for the leaf at |leaf_index|.
  // The size of the CT log tree must be provided in |tree_size|.
  // The log is identified by |domain_for_log|, which is the DNS name used as a
  // suffix for all queries.
  // The |callback| is invoked when the query is complete, or an error occurs.
  void QueryAuditProof(base::StringPiece domain_for_log,
                       uint64_t leaf_index,
                       uint64_t tree_size,
                       const AuditProofCallback& callback);

 private:
  void QueryLeafIndexComplete(net::DnsTransaction* transaction,
                              int neterror,
                              const net::DnsResponse* response);

  // Queries a CT log to retrieve part of an audit |proof|. The |node_index|
  // indicates which node of the audit proof/ should be requested. The CT log
  // may return up to 7 nodes, starting from |node_index| (this is the maximum
  // that will fit in a DNS UDP packet). The nodes will be appended to
  // |proof->nodes|.
  void QueryAuditProofNodes(std::unique_ptr<net::ct::MerkleAuditProof> proof,
                            base::StringPiece domain_for_log,
                            uint64_t tree_size,
                            uint64_t node_index,
                            const AuditProofCallback& callback);

  void QueryAuditProofNodesComplete(
      std::unique_ptr<net::ct::MerkleAuditProof> proof,
      base::StringPiece domain_for_log,
      uint64_t tree_size,
      net::DnsTransaction* transaction,
      int net_error,
      const net::DnsResponse* response);

  // Updates the |dns_client_| config using NetworkChangeNotifier.
  void UpdateDnsConfig();

  // A DNS query that is in flight.
  template <typename CallbackType>
  struct Query {
    std::unique_ptr<net::DnsTransaction> transaction;
    CallbackType callback;
  };

  // Used to perform DNS queries.
  std::unique_ptr<net::DnsClient> dns_client_;
  // Passed to the DNS client for logging.
  net::NetLogWithSource net_log_;
  // Leaf index queries that haven't completed yet.
  std::list<Query<LeafIndexCallback>> leaf_index_queries_;
  // Audit proof queries that haven't completed yet.
  std::list<Query<AuditProofCallback>> audit_proof_queries_;
  // Creates weak_ptrs to this, for callback purposes.
  base::WeakPtrFactory<LogDnsClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogDnsClient);
};

}  // namespace certificate_transparency
#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_DNS_CLIENT_H_
