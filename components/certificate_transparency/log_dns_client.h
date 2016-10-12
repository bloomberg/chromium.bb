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
  // A limit can be set on the number of concurrent DNS queries by providing a
  // positive value for |max_concurrent_queries|. Queries that would exceed this
  // limit will fail with net::TEMPORARILY_THROTTLED. Setting this to 0 will
  // disable this limit.
  LogDnsClient(std::unique_ptr<net::DnsClient> dns_client,
               const net::NetLogWithSource& net_log,
               size_t max_concurrent_queries);
  // Must be deleted on the same thread that it was created on.
  ~LogDnsClient() override;

  // Called by NetworkChangeNotifier when the DNS config changes.
  // The DnsClient's config will be updated in response.
  void OnDNSChanged() override;

  // Called by NetworkChangeNotifier when the DNS config is first read.
  // The DnsClient's config will be updated in response.
  void OnInitialDNSConfigRead() override;

  // Queries a CT log to retrieve an audit proof for the leaf with |leaf_hash|.
  // The |leaf_hash| is the SHA-256 Merkle leaf hash (see RFC6962, section 2.1).
  // The size of the CT log tree must be provided in |tree_size|.
  // The log is identified by |domain_for_log|, which is the DNS name used as a
  // suffix for all queries.
  // The |callback| is invoked when the query is complete, or an error occurs.
  void QueryAuditProof(const std::string& domain_for_log,
                       base::StringPiece leaf_hash,
                       uint64_t tree_size,
                       const AuditProofCallback& callback);

 private:
  // An audit proof query that is in progress.
  class AuditProofQuery;

  // Invoked when an audit proof query completes.
  // |callback| is the user-provided callback that should be notified.
  // |result| is a net::Error indicating success or failure.
  // |query| is the query that has completed.
  // The query is removed from |audit_proof_queries_| by this method.
  void QueryAuditProofComplete(const AuditProofCallback& callback,
                               int result,
                               AuditProofQuery* query);

  // Returns true if the maximum number of queries are currently in flight.
  // If the maximum number of concurrency queries is set to 0, this will always
  // return false.
  bool HasMaxConcurrentQueriesInProgress() const;

  // Updates the |dns_client_| config using NetworkChangeNotifier.
  void UpdateDnsConfig();

  // Used to perform DNS queries.
  std::unique_ptr<net::DnsClient> dns_client_;
  // Passed to the DNS client for logging.
  net::NetLogWithSource net_log_;
  // Audit proof queries that haven't completed yet.
  std::list<std::unique_ptr<AuditProofQuery>> audit_proof_queries_;
  // The maximum number of queries that can be in flight at one time.
  size_t max_concurrent_queries_;
  // Creates weak_ptrs to this, for callback purposes.
  base::WeakPtrFactory<LogDnsClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogDnsClient);
};

}  // namespace certificate_transparency
#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_DNS_CLIENT_H_
