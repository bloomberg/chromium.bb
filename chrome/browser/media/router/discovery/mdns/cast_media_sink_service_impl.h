// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/containers/small_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/discovery/media_sink_service_base.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_request_context_getter.h"

namespace cast_channel {
class CastSocketService;
}  // namespace cast_channel

namespace media_router {

// A service used to open/close cast channels for Cast devices. This class is
// not thread safe and should be invoked only on the IO thread.
class CastMediaSinkServiceImpl
    : public MediaSinkServiceBase,
      public cast_channel::CastSocket::Observer,
      public base::SupportsWeakPtr<CastMediaSinkServiceImpl>,
      public DiscoveryNetworkMonitor::Observer {
 public:
  using SinkSource = CastDeviceCountMetrics::SinkSource;

  // Default Cast control port to open Cast Socket from DIAL sink.
  static constexpr int kCastControlPort = 8009;

  // The max number of cast channel open failure for a DIAL-discovered sink
  // before we can say confidently that it is unlikely to be a Cast device.
  static constexpr int kMaxDialSinkFailureCount = 10;

  CastMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& callback,
      cast_channel::CastSocketService* cast_socket_service,
      DiscoveryNetworkMonitor* network_monitor,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~CastMediaSinkServiceImpl() override;

  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  void SetClockForTest(std::unique_ptr<base::Clock> clock);

  // MediaSinkService implementation
  void Start() override;
  void Stop() override;

  // MediaSinkServiceBase implementation
  // Called when the discovery loop timer expires.
  void OnFetchCompleted() override;
  void RecordDeviceCounts() override;

  // Opens cast channels on the IO thread.
  virtual void OpenChannels(const std::vector<MediaSinkInternal>& cast_sinks,
                            SinkSource sink_source);

  void OnDialSinkAdded(const MediaSinkInternal& sink);

  // Tries to open cast channels for sinks found by current round of mDNS
  // discovery, but without opened cast channels.
  // |cast_sinks|: list of sinks found by current round of mDNS discovery.
  void AttemptConnection(const std::vector<MediaSinkInternal>& cast_sinks);

 private:
  friend class CastMediaSinkServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelOpenSucceeded);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestMultipleOnChannelOpenSucceeded);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestTimer);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOpenChannelNoRetry);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOpenChannelRetryOnce);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOpenChannelFails);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestMultipleOpenChannels);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelOpenFailed);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorMayRetryForConnectingChannel);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorMayRetryForCastSink);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnChannelErrorNoRetryForMissingSink);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOnDialSinkAdded);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOnFetchCompleted);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheSinksForKnownNetwork);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheContainsOnlyResolvedSinks);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheUpdatedOnChannelOpenFailed);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, UnknownNetworkNoCache);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheUpdatedForKnownNetwork);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           CacheDialDiscoveredSinks);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           DualDiscoveryDoesntDuplicateCacheItems);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestAttemptConnection);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParameters);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithDefaultValue);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestCreateCastSocketOpenParams);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithFeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestInitParameters);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestInitRetryParametersWithDefaultValue);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestOnDialSinkAddedSkipsIfNonCastDevice);

  // CastSocket::Observer implementation.
  void OnError(const cast_channel::CastSocket& socket,
               cast_channel::ChannelError error_state) override;
  void OnMessage(const cast_channel::CastSocket& socket,
                 const cast_channel::CastMessage& message) override;

  // DiscoveryNetworkMonitor::Observer implementation
  void OnNetworksChanged(const std::string& network_id) override;

  // Returns cast socket open parameters. Parameters are read from Finch.
  // Connect / liveness timeout value are dynamically calculated
  // based on the channel's last error status.
  // |ip_endpoint|: ip endpoint of cast channel to be connected to.
  cast_channel::CastSocketOpenParams CreateCastSocketOpenParams(
      const net::IPEndPoint& ip_endpoint);

  // Opens cast channel.
  // |ip_endpoint|: cast channel's target IP endpoint.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry passed to |OnChannelOpened| callback.
  void OpenChannel(const net::IPEndPoint& ip_endpoint,
                   const MediaSinkInternal& cast_sink,
                   std::unique_ptr<net::BackoffEntry> backoff_entry,
                   SinkSource sink_source);

  // Invoked when opening cast channel on IO thread completes.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry passed to |OnChannelErrorMayRetry| callback
  // if open channel fails.
  // |start_time|: time at which corresponding |OpenChannel| was called.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpened(const MediaSinkInternal& cast_sink,
                       std::unique_ptr<net::BackoffEntry> backoff_entry,
                       SinkSource sink_source,
                       base::Time start_time,
                       cast_channel::CastSocket* socket);

  // Invoked by |OnChannelOpened| if opening cast channel failed. It will retry
  // opening channel in a delay specified by |backoff_entry| if current failure
  // count is less than max retry attempts. Or invoke |OnChannelError| if retry
  // is not allowed.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry holds failure count and calculates back-off
  // for next retry.
  // |error_state|: erorr encountered when opending cast channel.
  void OnChannelErrorMayRetry(MediaSinkInternal cast_sink,
                              std::unique_ptr<net::BackoffEntry> backoff_entry,
                              cast_channel::ChannelError error_state,
                              SinkSource sink_source);

  // Invoked when opening cast channel on IO thread succeeds.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpenSucceeded(MediaSinkInternal cast_sink,
                              cast_channel::CastSocket* socket,
                              SinkSource sink_source);

  // Invoked when opening cast channel on IO thread fails after all retry
  // attempts.
  // |ip_endpoint|: ip endpoint of cast channel failing to connect to.
  // |sink_source|: Method of sink discovery.
  void OnChannelOpenFailed(const net::IPEndPoint& ip_endpoint);

  // Returns whether the given DIAL-discovered |sink| is probably a non-Cast
  // device. This is heuristically determined by two things: |sink| has been
  // discovered via DIAL exclusively, and we failed to open a cast channel to
  // |sink| a number of times past a pre-determined threshold.
  // TODO(crbug.com/774233): This is a temporary and not a definitive way to
  // tell if a device is a Cast/non-Cast device. We need to collect some metrics
  // for the device description URL advertised by Cast devices to determine the
  // long term solution for restricting dual discovery.
  bool IsProbablyNonCastDevice(const MediaSinkInternal& sink) const;

  // Holds Finch field trial parameters controlling Cast channel retry strategy.
  struct RetryParams {
    // Initial delay (in ms) once backoff starts.
    int initial_delay_in_milliseconds;

    // Max retry attempts allowed when opening a Cast socket.
    int max_retry_attempts;

    // Factor by which the delay will be multiplied on each subsequent failure.
    // This must be >= 1.0.
    double multiply_factor;

    RetryParams();
    ~RetryParams();

    bool Validate();

    static RetryParams GetFromFieldTrialParam();
  };

  // Holds Finch field trial parameters controlling Cast channel open.
  struct OpenParams {
    // Connect timeout value when opening a Cast socket.
    int connect_timeout_in_seconds;

    // Amount of idle time to wait before pinging the Cast device.
    int ping_interval_in_seconds;

    // Amount of idle time to wait before disconnecting.
    int liveness_timeout_in_seconds;

    // Dynamic time out delta for connect timeout and liveness timeout. If
    // previous channel open operation with opening parameters (liveness
    // timeout, connect timeout) fails, next channel open will have parameters
    // (liveness timeout + delta, connect timeout + delta).
    int dynamic_timeout_delta_in_seconds;

    OpenParams();
    ~OpenParams();

    bool Validate();

    static OpenParams GetFromFieldTrialParam();
  };

  // Set of IP endpoints pending to be connected to.
  std::set<net::IPEndPoint> pending_for_open_ip_endpoints_;

  // Set of IP endpoints found in current round of mDNS service. Used by
  // RecordDeviceCounts().
  std::set<net::IPEndPoint> known_ip_endpoints_;

  using MediaSinkInternalMap = std::map<net::IPEndPoint, MediaSinkInternal>;

  // Map of sinks with opened cast channels keyed by IP endpoint.
  MediaSinkInternalMap current_sinks_map_;

  // Raw pointer of leaky singleton CastSocketService, which manages adding and
  // removing Cast channels.
  cast_channel::CastSocketService* const cast_socket_service_;

  // Raw pointer to DiscoveryNetworkMonitor, which is a global leaky singleton
  // and manages network change notifications.
  DiscoveryNetworkMonitor* network_monitor_;

  std::string current_network_id_ = DiscoveryNetworkMonitor::kNetworkIdUnknown;

  // Cache of known sinks by network ID.
  std::map<std::string, std::vector<MediaSinkInternal>> sink_cache_;

  CastDeviceCountMetrics metrics_;

  RetryParams retry_params_;

  OpenParams open_params_;

  net::BackoffEntry::Policy backoff_policy_;

  // Map of consecutive failure count keyed by IP endpoint. Keeps track of
  // failure counts for each IP endpoint. Used to dynamically adjust timeout
  // values. If a Cast channel opens successfully, it is removed from the map.
  std::map<net::IPEndPoint, int> failure_count_map_;

  // Used by |IsProbablyNonCastDevice()| to keep track of how many times we
  // failed to open a cast channel for a sink that is discovered via DIAL
  // exclusively. The count is reset for a sink when it is discovered via mDNS,
  // or if we detected a network change.
  base::small_map<std::map<net::IPAddress, int>> dial_sink_failure_count_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // This is a temporary workaround to get access to the net::NetLog* from the
  // NetworkService.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<base::Clock> clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
