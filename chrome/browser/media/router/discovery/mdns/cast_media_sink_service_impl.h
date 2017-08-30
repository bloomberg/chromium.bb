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
  static const int kCastControlPort;

  CastMediaSinkServiceImpl(const OnSinksDiscoveredCallback& callback,
                           cast_channel::CastSocketService* cast_socket_service,
                           DiscoveryNetworkMonitor* network_monitor);
  ~CastMediaSinkServiceImpl() override;

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

 private:
  friend class CastMediaSinkServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOnChannelOpened);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestMultipleOnChannelOpened);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestTimer);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest,
                           TestMultipleOpenChannels);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOnChannelError);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOnDialSinkAdded);
  FRIEND_TEST_ALL_PREFIXES(CastMediaSinkServiceImplTest, TestOnFetchCompleted);

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
  void OpenChannel(const net::IPEndPoint& ip_endpoint,
                   MediaSinkInternal cast_sink);

  // Invoked when opening cast channel on IO thread completes.
  // |cast_sink|: Cast sink created from mDNS service description or DIAL sink.
  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnChannelOpened(MediaSinkInternal cast_sink,
                       cast_channel::CastSocket* socket);

  // Set of mDNS service IP endpoints from current round of discovery.
  std::set<net::IPEndPoint> current_service_ip_endpoints_;

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

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_CAST_MEDIA_SINK_SERVICE_IMPL_H_
