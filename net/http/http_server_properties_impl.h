// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/http/broken_alternative_services.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"

namespace base {
class Clock;
class TickClock;
}

namespace net {

class NetLog;

// The implementation for setting/retrieving the HTTP server properties.
//
// Optionally retrieves and saves properties from/to disk using a
// HttpServerPropertiesManager::PrefDelegate. Delays and rate-limits writes to
// the file, to improve performance.
//
// TODO(mmenke): Merge this with HttpServerProperties.
class NET_EXPORT HttpServerPropertiesImpl
    : public HttpServerProperties,
      public BrokenAlternativeServices::Delegate {
 public:
  // If a |pref_delegate| is specified, it will be used to read/write the
  // properties to a pref file.
  //
  // |tick_clock| is used for setting expiration times and scheduling the
  // expiration of broken alternative services. If null, default clock will be
  // used.
  //
  // |clock| is used for converting base::TimeTicks to base::Time for
  // wherever base::Time is preferable.
  HttpServerPropertiesImpl(
      std::unique_ptr<HttpServerPropertiesManager::PrefDelegate> pref_delegate =
          nullptr,
      NetLog* net_log = nullptr,
      const base::TickClock* tick_clock = nullptr,
      base::Clock* clock = nullptr);

  ~HttpServerPropertiesImpl() override;

  // -----------------------------
  // HttpServerProperties methods:
  // -----------------------------

  void Clear(base::OnceClosure callback) override;
  bool SupportsRequestPriority(const url::SchemeHostPort& server) override;
  bool GetSupportsSpdy(const url::SchemeHostPort& server) override;
  void SetSupportsSpdy(const url::SchemeHostPort& server,
                       bool support_spdy) override;
  bool RequiresHTTP11(const HostPortPair& server) override;
  void SetHTTP11Required(const HostPortPair& server) override;
  void MaybeForceHTTP11(const HostPortPair& server,
                        SSLConfig* ssl_config) override;
  AlternativeServiceInfoVector GetAlternativeServiceInfos(
      const url::SchemeHostPort& origin) override;
  bool SetHttp2AlternativeService(const url::SchemeHostPort& origin,
                                  const AlternativeService& alternative_service,
                                  base::Time expiration) override;
  bool SetQuicAlternativeService(
      const url::SchemeHostPort& origin,
      const AlternativeService& alternative_service,
      base::Time expiration,
      const quic::ParsedQuicVersionVector& advertised_versions) override;
  bool SetAlternativeServices(const url::SchemeHostPort& origin,
                              const AlternativeServiceInfoVector&
                                  alternative_service_info_vector) override;
  void MarkAlternativeServiceBroken(
      const AlternativeService& alternative_service) override;
  void MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      const AlternativeService& alternative_service) override;
  void MarkAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) override;
  bool IsAlternativeServiceBroken(
      const AlternativeService& alternative_service) const override;
  bool WasAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) override;
  void ConfirmAlternativeService(
      const AlternativeService& alternative_service) override;
  bool OnDefaultNetworkChanged() override;
  const AlternativeServiceMap& alternative_service_map() const override;
  std::unique_ptr<base::Value> GetAlternativeServiceInfoAsValue()
      const override;
  bool GetSupportsQuic(IPAddress* last_address) const override;
  void SetSupportsQuic(bool used_quic, const IPAddress& address) override;
  void SetServerNetworkStats(const url::SchemeHostPort& server,
                             ServerNetworkStats stats) override;
  void ClearServerNetworkStats(const url::SchemeHostPort& server) override;
  const ServerNetworkStats* GetServerNetworkStats(
      const url::SchemeHostPort& server) override;
  const ServerNetworkStatsMap& server_network_stats_map() const override;
  bool SetQuicServerInfo(const quic::QuicServerId& server_id,
                         const std::string& server_info) override;
  const std::string* GetQuicServerInfo(
      const quic::QuicServerId& server_id) override;
  const QuicServerInfoMap& quic_server_info_map() const override;
  size_t max_server_configs_stored_in_properties() const override;
  void SetMaxServerConfigsStoredInProperties(
      size_t max_server_configs_stored_in_properties) override;
  bool IsInitialized() const override;

  // BrokenAlternativeServices::Delegate method.
  void OnExpireBrokenAlternativeService(
      const AlternativeService& expired_alternative_service) override;

  static base::TimeDelta GetUpdatePrefsDelayForTesting();

  // Test-only routines that call the methods used to load the specified
  // field(s) from a prefs file. Unlike OnPrefsLoaded(), these may be invoked
  // multiple times.
  void OnSpdyServersLoadedForTesting(
      std::unique_ptr<SpdyServersMap> spdy_servers_map) {
    OnSpdyServersLoaded(std::move(spdy_servers_map));
  }
  void OnAlternativeServiceServersLoadedForTesting(
      std::unique_ptr<AlternativeServiceMap> alternate_protocol_servers) {
    OnAlternativeServiceServersLoaded(std::move(alternate_protocol_servers));
  }
  void OnServerNetworkStatsLoadedForTesting(
      std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map) {
    OnServerNetworkStatsLoaded(std::move(server_network_stats_map));
  }
  void OnSupportsQuicLoadedForTesting(const IPAddress& last_address) {
    OnSupportsQuicLoaded(last_address);
  }
  void OnQuicServerInfoMapLoadedForTesting(
      std::unique_ptr<QuicServerInfoMap> quic_server_info_map) {
    OnQuicServerInfoMapLoaded(std::move(quic_server_info_map));
  }
  void OnBrokenAndRecentlyBrokenAlternativeServicesLoadedForTesting(
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services) {
    OnBrokenAndRecentlyBrokenAlternativeServicesLoaded(
        std::move(broken_alternative_service_list),
        std::move(recently_broken_alternative_services));
  }

  const std::string* GetCanonicalSuffixForTesting(
      const std::string& host) const {
    return GetCanonicalSuffix(host);
  }

  const SpdyServersMap& spdy_servers_map_for_testing() const {
    return spdy_servers_map_;
  }

  // TODO(mmenke): Look into removing this.
  HttpServerPropertiesManager* properties_manager_for_testing() {
    return properties_manager_.get();
  }

 private:
  // TODO (wangyix): modify HttpServerPropertiesImpl unit tests so this
  // friendness is no longer required.
  friend class HttpServerPropertiesImplPeer;

  typedef base::flat_map<url::SchemeHostPort, url::SchemeHostPort>
      CanonicalAltSvcMap;
  typedef base::flat_map<HostPortPair, quic::QuicServerId>
      CanonicalServerInfoMap;
  typedef std::vector<std::string> CanonicalSuffixList;
  typedef std::set<HostPortPair> Http11ServerHostPortSet;

  // Return the iterator for |server|, or for its canonical host, or end.
  AlternativeServiceMap::const_iterator GetAlternateProtocolIterator(
      const url::SchemeHostPort& server);

  // Return the canonical host for |server|, or end if none exists.
  CanonicalAltSvcMap::const_iterator GetCanonicalAltSvcHost(
      const url::SchemeHostPort& server) const;

  // Return the canonical host with the same canonical suffix as |server|.
  // The returned canonical host can be used to search for server info in
  // |quic_server_info_map_|. Return 'end' the host doesn't exist.
  CanonicalServerInfoMap::const_iterator GetCanonicalServerInfoHost(
      const quic::QuicServerId& server) const;

  // Remove the canonical alt-svc host for |server|.
  void RemoveAltSvcCanonicalHost(const url::SchemeHostPort& server);

  // Update |canonical_server_info_map_| with the new canonical host.
  // The |server| should have the corresponding server info associated with it
  // in |quic_server_info_map_|. If |canonical_server_info_map_| doesn't
  // have an entry associated with |server|, the method will add one.
  void UpdateCanonicalServerInfoMap(const quic::QuicServerId& server);

  // Returns the canonical host suffix for |host|, or nullptr if none
  // exists.
  const std::string* GetCanonicalSuffix(const std::string& host) const;

  void OnPrefsLoaded(
      std::unique_ptr<SpdyServersMap> spdy_servers_map,
      std::unique_ptr<AlternativeServiceMap> alternative_service_map,
      std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map,
      const IPAddress& last_quic_address,
      std::unique_ptr<QuicServerInfoMap> quic_server_info_map,
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services,
      bool prefs_corrupt);

  // These methods are called by OnPrefsLoaded to handle merging properties
  // loaded from prefs with what has been learned while waiting for prefs to
  // load.
  void OnSpdyServersLoaded(std::unique_ptr<SpdyServersMap> spdy_servers_map);
  void OnAlternativeServiceServersLoaded(
      std::unique_ptr<AlternativeServiceMap> alternate_protocol_servers);
  void OnServerNetworkStatsLoaded(
      std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map);
  void OnSupportsQuicLoaded(const IPAddress& last_address);
  void OnQuicServerInfoMapLoaded(
      std::unique_ptr<QuicServerInfoMap> quic_server_info_map);
  void OnBrokenAndRecentlyBrokenAlternativeServicesLoaded(
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services);

  // Queue a delayed call to WriteProperties(). If |is_initialized_| is false,
  // or |properties_manager_| is nullptr, or there's already a queued call to
  // WriteProperties(), does nothing.
  void MaybeQueueWriteProperties();

  // Writes cached state to |properties_manager_|, which must not be null.
  // Invokes |callback| on completion, if non-null.
  void WriteProperties(base::OnceClosure callback) const;

  const base::TickClock* tick_clock_;  // Unowned
  base::Clock* clock_;                 // Unowned

  // Set to true once initial properties have been retrieved from disk by
  // |properties_manager_|. Always true if |properties_manager_| is nullptr.
  bool is_initialized_;

  // Used to load/save properties from/to preferences. May be nullptr.
  std::unique_ptr<HttpServerPropertiesManager> properties_manager_;

  SpdyServersMap spdy_servers_map_;
  Http11ServerHostPortSet http11_servers_;

  AlternativeServiceMap alternative_service_map_;

  BrokenAlternativeServices broken_alternative_services_;

  IPAddress last_quic_address_;
  ServerNetworkStatsMap server_network_stats_map_;
  // Contains a map of servers which could share the same alternate protocol.
  // Map from a Canonical scheme/host/port (host is some postfix of host names)
  // to an actual origin, which has a plausible alternate protocol mapping.
  CanonicalAltSvcMap canonical_alt_svc_map_;

  // Contains list of suffixes (for example ".c.youtube.com",
  // ".googlevideo.com", ".googleusercontent.com") of canonical hostnames.
  const CanonicalSuffixList canonical_suffixes_;

  QuicServerInfoMap quic_server_info_map_;

  // Maps canonical suffixes to host names that have the same canonical suffix
  // and have a corresponding entry in |quic_server_info_map_|. The map can be
  // used to quickly look for server info for hosts that share the same
  // canonical suffix but don't have exact match in |quic_server_info_map_|. The
  // map exists solely to improve the search performance. It only contains
  // derived data that can be recalculated by traversing
  // |quic_server_info_map_|.
  CanonicalServerInfoMap canonical_server_info_map_;

  size_t max_server_configs_stored_in_properties_;

  // Used to post calls to WriteProperties().
  base::OneShotTimer prefs_update_timer_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
