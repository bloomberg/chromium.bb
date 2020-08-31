// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_RESOLVE_CONTEXT_H_
#define NET_DNS_RESOLVE_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/metrics/sample_vector.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/isolation_info.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config.h"

namespace net {

class ClassicDnsServerIterator;
class DnsSession;
class DnsServerIterator;
class DohDnsServerIterator;
class HostCache;
class URLRequestContext;

// Per-URLRequestContext data used by HostResolver. Expected to be owned by the
// ContextHostResolver, and all usage/references are expected to be cleaned up
// or cancelled before the URLRequestContext goes out of service.
class NET_EXPORT_PRIVATE ResolveContext : public base::CheckedObserver {
 public:
  // Number of failures allowed before a DoH server is designated 'unavailable'.
  // In AUTOMATIC mode, non-probe DoH queries should not be sent to DoH servers
  // that have reached this limit.
  //
  // This limit is different from the failure limit that governs insecure async
  // resolver bypass in multiple ways: NXDOMAIN responses are never counted as
  // failures, and the outcome of fallback queries is not taken into account.
  static const int kAutomaticModeFailureLimit = 10;

  class DohStatusObserver : public base::CheckedObserver {
   public:
    // Notification indicating that the current session for which DoH servers
    // are being tracked has changed.
    virtual void OnSessionChanged() = 0;

    // Notification indicating that a DoH server has been marked unavailable,
    // but is ready for usage such as availability probes.
    //
    // |network_change| true if the invalidation was triggered by a network
    // connection change.
    virtual void OnDohServerUnavailable(bool network_change) = 0;

   protected:
    DohStatusObserver() = default;
    ~DohStatusObserver() override = default;
  };

  ResolveContext(URLRequestContext* url_request_context, bool enable_caching);

  ResolveContext(const ResolveContext&) = delete;
  ResolveContext& operator=(const ResolveContext&) = delete;

  ~ResolveContext() override;

  // Returns an iterator for DoH DNS servers.
  std::unique_ptr<DnsServerIterator> GetDohIterator(
      const DnsConfig& config,
      const DnsConfig::SecureDnsMode& mode,
      const DnsSession* session);

  // Returns an iterator for classic DNS servers.
  std::unique_ptr<DnsServerIterator> GetClassicDnsIterator(
      const DnsConfig& config,
      const DnsSession* session);

  // Returns whether |doh_server_index| is eligible for use in AUTOMATIC mode,
  // that is that consecutive failures are less than kAutomaticModeFailureLimit
  // and the server has had at least one successful query or probe. Always
  // |false| if |session| is not the current session.
  bool GetDohServerAvailability(size_t doh_server_index,
                                const DnsSession* session) const;

  // Returns the number of DoH servers available for use in AUTOMATIC mode (see
  // GetDohServerAvailability()). Always 0 if |session| is not the current
  // session.
  size_t NumAvailableDohServers(const DnsSession* session) const;

  // Record that server failed to respond (due to SRV_FAIL or timeout). If
  // |is_doh_server| and the number of failures has surpassed a threshold,
  // sets the DoH probe state to unavailable. Noop if |session| is not the
  // current session.
  void RecordServerFailure(size_t server_index,
                           bool is_doh_server,
                           const DnsSession* session);

  // Record that server responded successfully. Noop if |session| is not the
  // current session.
  void RecordServerSuccess(size_t server_index,
                           bool is_doh_server,
                           const DnsSession* session);

  // Record how long it took to receive a response from the server. Noop if
  // |session| is not the current session.
  void RecordRtt(size_t server_index,
                 bool is_doh_server,
                 base::TimeDelta rtt,
                 int rv,
                 const DnsSession* session);

  // Return the timeout for the next query. |attempt| counts from 0 and is used
  // for exponential backoff.
  base::TimeDelta NextClassicTimeout(size_t classic_server_index,
                                     int attempt,
                                     const DnsSession* session);

  // Return the timeout for the next DoH query.
  base::TimeDelta NextDohTimeout(size_t doh_server_index,
                                 const DnsSession* session);

  void RegisterDohStatusObserver(DohStatusObserver* observer);
  void UnregisterDohStatusObserver(const DohStatusObserver* observer);

  URLRequestContext* url_request_context() { return url_request_context_; }
  void set_url_request_context(URLRequestContext* url_request_context) {
    DCHECK(!url_request_context_);
    DCHECK(url_request_context);
    url_request_context_ = url_request_context;
  }

  HostCache* host_cache() { return host_cache_.get(); }

  // Invalidate or clear saved per-context cached data that is not expected to
  // stay valid between connections or sessions (eg the HostCache and DNS server
  // stats). |new_session|, if non-null, will be the new "current" session for
  // which per-session data will be kept.
  void InvalidateCachesAndPerSessionData(const DnsSession* new_session,
                                         bool network_change);

  const DnsSession* current_session_for_testing() const {
    return current_session_.get();
  }

  // Returns IsolationInfo that should be used for DoH requests. Using a single
  // transient IsolationInfo ensures that DNS requests aren't pooled with normal
  // web requests, but still allows them to be pooled with each other, to allow
  // reusing connections to the DoH server across different third party
  // contexts. One downside of a transient IsolationInfo is that it means
  // metadata about the DoH server itself will not be cached across restarts
  // (alternative service info if it supports QUIC, for instance).
  const IsolationInfo& isolation_info() const { return isolation_info_; }

 private:
  friend DohDnsServerIterator;
  friend ClassicDnsServerIterator;
  // Runtime statistics of DNS server.
  struct ServerStats {
    explicit ServerStats(std::unique_ptr<base::SampleVector> rtt_histogram);

    ServerStats(ServerStats&&);

    ~ServerStats();

    // Count of consecutive failures after last success.
    int last_failure_count;

    // True if any success has ever been recorded for this server for the
    // current connection.
    bool current_connection_success = false;

    // Last time when server returned failure or timeout.
    base::TimeTicks last_failure;
    // Last time when server returned success.
    base::TimeTicks last_success;

    // A histogram of observed RTT .
    std::unique_ptr<base::SampleVector> rtt_histogram;
  };

  // Return the (potentially rotating) index of the first configured server (to
  // be passed to [Doh]ServerIndexToUse()). Always returns 0 if |session| is not
  // the current session.
  size_t FirstServerIndex(bool doh_server, const DnsSession* session);

  bool IsCurrentSession(const DnsSession* session) const;

  // Returns the ServerStats for the designated server. Returns nullptr if no
  // ServerStats found.
  ServerStats* GetServerStats(size_t server_index, bool is_doh_server);

  // Return the timeout for the next query.
  base::TimeDelta NextTimeoutHelper(ServerStats* server_stats, int attempt);

  // Record the time to perform a query.
  void RecordRttForUma(size_t server_index,
                       bool is_doh_server,
                       base::TimeDelta rtt,
                       int rv,
                       const DnsSession* session);

  void NotifyDohStatusObserversOfSessionChanged();
  void NotifyDohStatusObserversOfUnavailable(bool network_change);

  static bool ServerStatsToDohAvailability(const ServerStats& stats);

  URLRequestContext* url_request_context_;

  std::unique_ptr<HostCache> host_cache_;

  // Current maximum server timeout. Updated on connection change.
  base::TimeDelta max_timeout_;

  base::ObserverList<DohStatusObserver,
                     true /* check_empty */,
                     false /* allow_reentrancy */>
      doh_status_observers_;

  // Per-session data is only stored and valid for the latest session. Before
  // accessing, should check that |current_session_| is valid and matches a
  // passed in DnsSession.
  //
  // Using a WeakPtr, so even if a new session has the same pointer as an old
  // invalidated session, it can be recognized as a different session.
  //
  // TODO(crbug.com/1022059): Make const DnsSession once server stats have been
  // moved and no longer need to be read from DnsSession for availability logic.
  base::WeakPtr<const DnsSession> current_session_;
  // Current index into |config_.nameservers| to begin resolution with.
  int classic_server_index_ = 0;
  base::TimeDelta initial_timeout_;
  // Track runtime statistics of each classic (insecure) DNS server.
  std::vector<ServerStats> classic_server_stats_;
  // Track runtime statistics of each DoH server.
  std::vector<ServerStats> doh_server_stats_;

  const IsolationInfo isolation_info_;
};

}  // namespace net

#endif  // NET_DNS_RESOLVE_CONTEXT_H_
