// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_PROBER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_PROBER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "url/gurl.h"

namespace network {
class NetworkConnectionTracker;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// This class is a utility to probe a given URL with a given set of behaviors.
// This can be used for determining whether a specific network resource is
// available or accessible by Chrome.
// This class may live on either UI or IO thread but should remain on the thread
// that it was created on.
class PreviewsProber
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // This enum describes the different algorithms that can be used to calculate
  // a time delta between probe events like retries or timeout ttl.
  enum class Backoff {
    // Use the same time delta for each event.
    kLinear,

    // Use an exponentially increasing time delta, base 2.
    kExponential,
  };

  struct RetryPolicy {
    RetryPolicy();
    RetryPolicy(const RetryPolicy& other);
    ~RetryPolicy();

    // The maximum number of retries (not including the original probe) to
    // attempt.
    size_t max_retries = 3;

    // How to compute the time interval between successive retries.
    Backoff backoff = Backoff::kLinear;

    // Time between probes as the base value. For example, given |backoff|:
    //   LINEAR: |base_interval| between the end of last probe and start of next
    //           probe.
    //   EXPONENTIAL: (|base_interval| * 2 ^ |successive_retry_count_|) between
    //                the end of last retry and start of next retry.
    base::TimeDelta base_interval = base::TimeDelta();

    // If true, this attaches a random GUID query param to the URL of every
    // probe, including the first probe.
    bool use_random_urls = false;
  };

  struct TimeoutPolicy {
    TimeoutPolicy();
    TimeoutPolicy(const TimeoutPolicy& other);
    ~TimeoutPolicy();

    // How to compute the TTL of probes.
    Backoff backoff = Backoff::kLinear;

    // The TTL base value. For example,
    //   LINEAR: Each probe times out in |base_timeout|.
    //   EXPONENTIAL: Each probe times out in
    //                (|base_timeout| * 2 ^ |successive_timeout_count_|).
    base::TimeDelta base_timeout = base::TimeDelta::FromSeconds(60);
  };

  enum class HttpMethod {
    kGet,
    kHead,
  };

  PreviewsProber(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& name,
      const GURL& url,
      HttpMethod http_method,
      const net::HttpRequestHeaders headers,
      const RetryPolicy& retry_policy,
      const TimeoutPolicy& timeout_policy);
  ~PreviewsProber() override;

  // Sends a probe now if the prober is currently inactive. If the probe is
  // active (i.e.: there are probes in flight), this is a no-op.
  void SendNowIfInactive();

  // Returns the successfulness of the last probe, if there was one.
  base::Optional<bool> LastProbeWasSuccessful() const;

  // True if probes are being attempted, including retries.
  bool is_active() const { return is_active_; }

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 protected:
  // Exposes |tick_clock| for testing.
  PreviewsProber(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& name,
      const GURL& url,
      HttpMethod http_method,
      const net::HttpRequestHeaders headers,
      const RetryPolicy& retry_policy,
      const TimeoutPolicy& timeout_policy,
      const base::TickClock* tick_clock);

 private:
  void ResetState();
  void CreateAndStartURLLoader();
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);
  void ProcessProbeTimeout();
  void ProcessProbeFailure();
  void ProcessProbeSuccess();
  void AddSelfAsNetworkConnectionObserver(
      network::NetworkConnectionTracker* network_connection_tracker);

  // The name given to this prober instance, used in metrics, prefs, and traffic
  // annotations.
  const std::string name_;

  // The URL that will be probed.
  const GURL url_;

  // The HTTP method used for probing.
  const HttpMethod http_method_;

  // Additional headers to send on every probe. These are subject to CORS
  // checks.
  const net::HttpRequestHeaders headers_;

  // The retry policy to use in this prober.
  const RetryPolicy retry_policy_;

  // The timeout policy to use in this prober.
  const TimeoutPolicy timeout_policy_;

  // The number of retries that have been attempted. This count does not include
  // the original probe.
  size_t successive_retry_count_;

  // The number of timeouts that have occurred.
  size_t successive_timeout_count_;

  // If a retry is being attempted, this will be running until the next attempt.
  std::unique_ptr<base::OneShotTimer> retry_timer_;

  // If a probe is being attempted, this will be running until the TTL.
  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // The tick clock used within this class.
  const base::TickClock* tick_clock_;

  // Whether the prober is currently sending probes.
  bool is_active_;

  // The status of the last completed probe, if any.
  base::Optional<bool> last_probe_status_;

  // This reference is kept around for unregistering |this| as an observer on
  // any thread.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Used for setting up the |url_loader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URLLoader used for the probe. Expected to be non-null iff |is_active_|.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsProber> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsProber);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_PROBER_H_
