// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_

#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/discovery_network_monitor.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/discovery/media_sink_service_base.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket.h"
#include "net/base/backoff_entry.h"

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
  // Default Cast control port to open Cast Socket from DIAL sink.
  static constexpr int kCastControlPort = 8009;
  // TODO(zhaobin): Remove this when we switch to use max delay instead of max
  // number of retry attempts to decide when to stop retry.
  static constexpr int kMaxAttempts = 3;
  // Initial delay (in seconds) used by backoff policy.
  static constexpr int kDelayInSeconds = 15;

  CastMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& callback,
      cast_channel::CastSocketService* cast_socket_service,
      DiscoveryNetworkMonitor* network_monitor,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~CastMediaSinkServiceImpl() override;

  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // MediaSinkService implementation
  void Start() override;
  void Stop() override;

  // MediaSinkServiceBase implementation
  // Called when the discovery loop timer expires.
  void OnFetchCompleted() override;
  void RecordDeviceCounts() override;

  // Opens cast channels on the IO thread.
  virtual void OpenChannels(std::vector<MediaSinkInternal> cast_sinks);

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

  // CastSocket::Observer implementation.
  void OnError(const cast_channel::CastSocket& socket,
               cast_channel::ChannelError error_state) override;
  void OnMessage(const cast_channel::CastSocket& socket,
                 const cast_channel::CastMessage& message) override;

  // DiscoveryNetworkMonitor::Observer implementation
  void OnNetworksChanged(const std::string& network_id) override;

  // Opens cast channel.
  // |ip_endpoint|: cast channel's target IP endpoint.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry passed to |OnChannelOpened| callback.
  void OpenChannel(const net::IPEndPoint& ip_endpoint,
                   const MediaSinkInternal& cast_sink,
                   std::unique_ptr<net::BackoffEntry> backoff_entry);

  // Invoked when opening cast channel on IO thread completes.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |backoff_entry|: backoff entry passed to |OnChannelErrorMayRetry| callback
  // if open channel fails.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpened(const MediaSinkInternal& cast_sink,
                       std::unique_ptr<net::BackoffEntry> backoff_entry,
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
                              cast_channel::ChannelError error_state);

  // Invoked when opening cast channel on IO thread succeeds.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpenSucceeded(MediaSinkInternal cast_sink,
                              cast_channel::CastSocket* socket);

  // Invoked when opening cast channel on IO thread fails after all retry
  // attempts.
  // |ip_endpoint|: ip endpoint of cast channel failing to connect to.
  void OnChannelOpenFailed(const net::IPEndPoint& ip_endpoint);

  // Set of IP endpoints pending to be connected to.
  std::set<net::IPEndPoint> pending_for_open_ip_endpoints_;

  // Set of IP endpoints found in current round of mDNS service. Used by
  // RecordDeviceCounts().
  std::set<net::IPEndPoint> known_ip_endpoints_;

  using MediaSinkInternalMap = std::map<net::IPAddress, MediaSinkInternal>;

  // Map of sinks with opened cast channels keyed by IP address.
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

  // Default backoff policy to reopen Cast Socket when channel error occurs.
  static const net::BackoffEntry::Policy kBackoffPolicy;

  // Does not take ownership of |backoff_policy_|.
  const net::BackoffEntry::Policy* backoff_policy_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
