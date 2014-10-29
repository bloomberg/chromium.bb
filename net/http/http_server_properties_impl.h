// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/threading/non_thread_safe.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_server_properties.h"

namespace base {
class ListValue;
}

namespace net {

// The implementation for setting/retrieving the HTTP server properties.
class NET_EXPORT HttpServerPropertiesImpl
    : public HttpServerProperties,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  HttpServerPropertiesImpl();
  ~HttpServerPropertiesImpl() override;

  // Initializes |spdy_servers_map_| with the servers (host/port) from
  // |spdy_servers| that either support SPDY or not.
  void InitializeSpdyServers(std::vector<std::string>* spdy_servers,
                             bool support_spdy);

  void InitializeAlternateProtocolServers(
      AlternateProtocolMap* alternate_protocol_servers);

  void InitializeSpdySettingsServers(SpdySettingsMap* spdy_settings_map);

  void InitializeSupportsQuic(SupportsQuicMap* supports_quic_map);

  // Get the list of servers (host/port) that support SPDY. The max_size is the
  // number of MRU servers that support SPDY that are to be returned.
  void GetSpdyServerList(base::ListValue* spdy_server_list,
                         size_t max_size) const;

  // Returns flattened string representation of the |host_port_pair|. Used by
  // unittests.
  static std::string GetFlattenedSpdyServer(
      const net::HostPortPair& host_port_pair);

  // Debugging to simulate presence of an AlternateProtocol.
  // If we don't have an alternate protocol in the map for any given host/port
  // pair, force this ProtocolPortPair.
  static void ForceAlternateProtocol(const AlternateProtocolInfo& pair);
  static void DisableForcedAlternateProtocol();

  // Returns the canonical host suffix for |server|, or std::string() if none
  // exists.
  std::string GetCanonicalSuffix(const net::HostPortPair& server);

  // -----------------------------
  // HttpServerProperties methods:
  // -----------------------------

  // Gets a weak pointer for this object.
  base::WeakPtr<HttpServerProperties> GetWeakPtr() override;

  // Deletes all data.
  void Clear() override;

  // Returns true if |server| supports SPDY.
  bool SupportsSpdy(const HostPortPair& server) override;

  // Add |server| into the persistent store.
  void SetSupportsSpdy(const HostPortPair& server, bool support_spdy) override;

  // Returns true if |server| has an Alternate-Protocol header.
  bool HasAlternateProtocol(const HostPortPair& server) override;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  AlternateProtocolInfo GetAlternateProtocol(
      const HostPortPair& server) override;

  // Sets the Alternate-Protocol for |server|.
  void SetAlternateProtocol(const HostPortPair& server,
                            uint16 alternate_port,
                            AlternateProtocol alternate_protocol,
                            double probability) override;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  void SetBrokenAlternateProtocol(const HostPortPair& server) override;

  // Returns true if Alternate-Protocol for |server| was recently BROKEN.
  bool WasAlternateProtocolRecentlyBroken(const HostPortPair& server) override;

  // Confirms that Alternate-Protocol for |server| is working.
  void ConfirmAlternateProtocol(const HostPortPair& server) override;

  // Clears the Alternate-Protocol for |server|.
  void ClearAlternateProtocol(const HostPortPair& server) override;

  // Returns all Alternate-Protocol mappings.
  const AlternateProtocolMap& alternate_protocol_map() const override;

  void SetAlternateProtocolProbabilityThreshold(double threshold) override;

  // Gets a reference to the SettingsMap stored for a host.
  // If no settings are stored, returns an empty SettingsMap.
  const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) override;

  // Saves an individual SPDY setting for a host. Returns true if SPDY setting
  // is to be persisted.
  bool SetSpdySetting(const HostPortPair& host_port_pair,
                      SpdySettingsIds id,
                      SpdySettingsFlags flags,
                      uint32 value) override;

  // Clears all entries in |spdy_settings_map_| for a host.
  void ClearSpdySettings(const HostPortPair& host_port_pair) override;

  // Clears all entries in |spdy_settings_map_|.
  void ClearAllSpdySettings() override;

  // Returns all persistent SPDY settings.
  const SpdySettingsMap& spdy_settings_map() const override;

  // Methods for SupportsQuic.
  SupportsQuic GetSupportsQuic(
      const HostPortPair& host_port_pair) const override;

  void SetSupportsQuic(const HostPortPair& host_port_pair,
                       bool used_quic,
                       const std::string& address) override;

  const SupportsQuicMap& supports_quic_map() const override;

  // Methods for NetworkStats.
  void SetServerNetworkStats(const HostPortPair& host_port_pair,
                             NetworkStats stats) override;

  const NetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) const override;

 private:
  // |spdy_servers_map_| has flattened representation of servers (host, port)
  // that either support or not support SPDY protocol.
  typedef base::MRUCache<std::string, bool> SpdyServerHostPortMap;
  typedef std::map<HostPortPair, NetworkStats> ServerNetworkStatsMap;
  typedef std::map<HostPortPair, HostPortPair> CanonicalHostMap;
  typedef std::vector<std::string> CanonicalSufficList;
  // List of broken host:ports and the times when they can be expired.
  struct BrokenAlternateProtocolEntry {
    HostPortPair server;
    base::TimeTicks when;
  };
  typedef std::list<BrokenAlternateProtocolEntry>
      BrokenAlternateProtocolList;
  // Map from host:port to the number of times alternate protocol has
  // been marked broken.
  typedef std::map<HostPortPair, int> BrokenAlternateProtocolMap;

  // Return the canonical host for |server|, or end if none exists.
  CanonicalHostMap::const_iterator GetCanonicalHost(HostPortPair server) const;

  void RemoveCanonicalHost(const HostPortPair& server);
  void ExpireBrokenAlternateProtocolMappings();
  void ScheduleBrokenAlternateProtocolMappingsExpiration();

  SpdyServerHostPortMap spdy_servers_map_;

  AlternateProtocolMap alternate_protocol_map_;
  BrokenAlternateProtocolList broken_alternate_protocol_list_;
  BrokenAlternateProtocolMap broken_alternate_protocol_map_;

  SpdySettingsMap spdy_settings_map_;
  SupportsQuicMap supports_quic_map_;
  ServerNetworkStatsMap server_network_stats_map_;
  // Contains a map of servers which could share the same alternate protocol.
  // Map from a Canonical host/port (host is some postfix of host names) to an
  // actual origin, which has a plausible alternate protocol mapping.
  CanonicalHostMap canonical_host_to_origin_map_;
  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com", ".googleusercontent.com") of canonical hostnames.
  CanonicalSufficList canonical_suffixes_;

  double alternate_protocol_probability_threshold_;

  base::WeakPtrFactory<HttpServerPropertiesImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_IMPL_H_
