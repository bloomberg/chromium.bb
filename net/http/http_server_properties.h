// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_H_

#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/callback.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/http/alternative_service.h"
#include "net/third_party/quiche/src/quic/core/quic_bandwidth.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"  // TODO(willchan): Reconsider this.
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"
#include "url/scheme_host_port.h"

namespace base {
class Value;
}

namespace net {

class HostPortPair;
class IPAddress;
struct SSLConfig;

struct NET_EXPORT SupportsQuic {
  SupportsQuic() : used_quic(false) {}
  SupportsQuic(bool used_quic, const std::string& address)
      : used_quic(used_quic),
        address(address) {}

  bool Equals(const SupportsQuic& other) const {
    return used_quic == other.used_quic && address == other.address;
  }

  bool used_quic;
  std::string address;
};

struct NET_EXPORT ServerNetworkStats {
  ServerNetworkStats() : bandwidth_estimate(quic::QuicBandwidth::Zero()) {}

  bool operator==(const ServerNetworkStats& other) const {
    return srtt == other.srtt && bandwidth_estimate == other.bandwidth_estimate;
  }

  bool operator!=(const ServerNetworkStats& other) const {
    return !this->operator==(other);
  }

  base::TimeDelta srtt;
  quic::QuicBandwidth bandwidth_estimate;
};

typedef std::vector<AlternativeService> AlternativeServiceVector;
typedef std::vector<AlternativeServiceInfo> AlternativeServiceInfoVector;

// Store at most 300 MRU SupportsSpdyServerHostPortPairs in memory and disk.
const int kMaxSupportsSpdyServerEntries = 300;

// Store at most 200 MRU AlternateProtocolHostPortPairs in memory and disk.
const int kMaxAlternateProtocolEntries = 200;

// Store at most 200 MRU ServerNetworkStats in memory and disk.
const int kMaxServerNetworkStatsEntries = 200;

// Store at most 200 MRU RecentlyBrokenAlternativeServices in memory and disk.
// This ideally would be with the other constants in HttpServerProperties, but
// has to go here instead of prevent a circular dependency.
const int kMaxRecentlyBrokenAlternativeServiceEntries = 200;

// Store at most 5 MRU QUIC servers by default. This is mainly used by cronet.
const int kDefaultMaxQuicServerEntries = 5;

// Stores flattened representation of servers (scheme, host, port) and whether
// or not they support SPDY.
class SpdyServersMap : public base::MRUCache<std::string, bool> {
 public:
  SpdyServersMap()
      : base::MRUCache<std::string, bool>(kMaxSupportsSpdyServerEntries) {}
};

class AlternativeServiceMap
    : public base::MRUCache<url::SchemeHostPort, AlternativeServiceInfoVector> {
 public:
  AlternativeServiceMap()
      : base::MRUCache<url::SchemeHostPort, AlternativeServiceInfoVector>(
            kMaxAlternateProtocolEntries) {}
};

class ServerNetworkStatsMap
    : public base::MRUCache<url::SchemeHostPort, ServerNetworkStats> {
 public:
  ServerNetworkStatsMap()
      : base::MRUCache<url::SchemeHostPort, ServerNetworkStats>(
            kMaxServerNetworkStatsEntries) {}
};

// Max number of quic servers to store is not hardcoded and can be set.
// Because of this, QuicServerInfoMap will not be a subclass of MRUCache.
typedef base::MRUCache<quic::QuicServerId, std::string> QuicServerInfoMap;

// The interface for setting/retrieving the HTTP server properties.
// Currently, this class manages servers':
// * HTTP/2 support;
// * Alternative Service support;
// * QUIC data (like ServerNetworkStats and QuicServerInfo).
//
// Embedders must ensure that HttpServerProperites is completely initialized
// before the first request is issued.
class NET_EXPORT HttpServerProperties {
 public:
  HttpServerProperties() {}
  virtual ~HttpServerProperties() {}

  // Deletes all data. If |callback| is non-null, flushes data to disk
  // and invokes the callback asynchronously once changes have been written to
  // disk.
  virtual void Clear(base::OnceClosure callback) = 0;

  // Returns true if |server| supports a network protocol which honors
  // request prioritization.
  // Note that this also implies that the server supports request
  // multiplexing, since priorities imply a relationship between
  // multiple requests.
  virtual bool SupportsRequestPriority(const url::SchemeHostPort& server) = 0;

  // Returns the value set by SetSupportsSpdy(). If not set, returns false.
  virtual bool GetSupportsSpdy(const url::SchemeHostPort& server) = 0;

  // Add |server| into the persistent store. Should only be called from IO
  // thread.
  virtual void SetSupportsSpdy(const url::SchemeHostPort& server,
                               bool support_spdy) = 0;

  // Returns true if |server| has required HTTP/1.1 via HTTP/2 error code.
  virtual bool RequiresHTTP11(const HostPortPair& server) = 0;

  // Require HTTP/1.1 on subsequent connections.  Not persisted.
  virtual void SetHTTP11Required(const HostPortPair& server) = 0;

  // Modify SSLConfig to force HTTP/1.1.
  static void ForceHTTP11(SSLConfig* ssl_config);

  // Modify SSLConfig to force HTTP/1.1 if necessary.
  virtual void MaybeForceHTTP11(const HostPortPair& server,
                                SSLConfig* ssl_config) = 0;

  // Return all alternative services for |origin|, including broken ones.
  // Returned alternative services never have empty hostnames.
  virtual AlternativeServiceInfoVector GetAlternativeServiceInfos(
      const url::SchemeHostPort& origin) = 0;

  // Set a single HTTP/2 alternative service for |origin|.  Previous
  // alternative services for |origin| are discarded.
  // |alternative_service.host| may be empty.
  // Return true if |alternative_service_map_| has changed significantly enough
  // that it should be persisted to disk.
  virtual bool SetHttp2AlternativeService(
      const url::SchemeHostPort& origin,
      const AlternativeService& alternative_service,
      base::Time expiration) = 0;

  // Set a single QUIC alternative service for |origin|.  Previous alternative
  // services for |origin| are discarded.
  // |alternative_service.host| may be empty.
  // Return true if |alternative_service_map_| has changed significantly enough
  // that it should be persisted to disk.
  virtual bool SetQuicAlternativeService(
      const url::SchemeHostPort& origin,
      const AlternativeService& alternative_service,
      base::Time expiration,
      const quic::ParsedQuicVersionVector& advertised_versions) = 0;

  // Set alternative services for |origin|.  Previous alternative services for
  // |origin| are discarded.
  // Hostnames in |alternative_service_info_vector| may be empty.
  // |alternative_service_info_vector| may be empty.
  // Return true if |alternative_service_map_| has changed significantly enough
  // that it should be persisted to disk.
  virtual bool SetAlternativeServices(
      const url::SchemeHostPort& origin,
      const AlternativeServiceInfoVector& alternative_service_info_vector) = 0;

  // Marks |alternative_service| as broken.
  // |alternative_service.host| must not be empty.
  virtual void MarkAlternativeServiceBroken(
      const AlternativeService& alternative_service) = 0;

  // Marks |alternative_service| as broken until the default network changes.
  // |alternative_service.host| must not be empty.
  virtual void MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      const AlternativeService& alternative_service) = 0;

  // Marks |alternative_service| as recently broken.
  // |alternative_service.host| must not be empty.
  virtual void MarkAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) = 0;

  // Returns true iff |alternative_service| is currently broken.
  // |alternative_service.host| must not be empty.
  virtual bool IsAlternativeServiceBroken(
      const AlternativeService& alternative_service) const = 0;

  // Returns true iff |alternative_service| was recently broken.
  // |alternative_service.host| must not be empty.
  virtual bool WasAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) = 0;

  // Confirms that |alternative_service| is working.
  // |alternative_service.host| must not be empty.
  virtual void ConfirmAlternativeService(
      const AlternativeService& alternative_service) = 0;

  // Called when the default network changes.
  // Clears all the alternative services that were marked broken until the
  // default network changed.
  // Returns true if there is any broken alternative service affected by the
  // default network change.
  virtual bool OnDefaultNetworkChanged() = 0;

  // Returns all alternative service mappings.
  // Returned alternative services may have empty hostnames.
  virtual const AlternativeServiceMap& alternative_service_map() const = 0;

  // Returns all alternative service mappings as human readable strings.
  // Empty alternative service hostnames will be printed as such.
  virtual std::unique_ptr<base::Value> GetAlternativeServiceInfoAsValue()
      const = 0;

  virtual bool GetSupportsQuic(IPAddress* last_address) const = 0;

  virtual void SetSupportsQuic(bool used_quic,
                               const IPAddress& last_address) = 0;

  // Sets |stats| for |server|.
  virtual void SetServerNetworkStats(const url::SchemeHostPort& server,
                                     ServerNetworkStats stats) = 0;

  // Clears any stats for |server|.
  virtual void ClearServerNetworkStats(const url::SchemeHostPort& server) = 0;

  // Returns any stats for |server| or nullptr if there are none.
  virtual const ServerNetworkStats* GetServerNetworkStats(
      const url::SchemeHostPort& server) = 0;

  virtual const ServerNetworkStatsMap& server_network_stats_map() const = 0;

  // Save QuicServerInfo (in std::string form) for the given |server_id|.
  // Returns true if the value has changed otherwise it returns false.
  virtual bool SetQuicServerInfo(const quic::QuicServerId& server_id,
                                 const std::string& server_info) = 0;

  // Get QuicServerInfo (in std::string form) for the given |server_id|.
  virtual const std::string* GetQuicServerInfo(
      const quic::QuicServerId& server_id) = 0;

  // Returns all persistent QuicServerInfo objects.
  virtual const QuicServerInfoMap& quic_server_info_map() const = 0;

  // Returns the number of server configs (QuicServerInfo objects) persisted.
  virtual size_t max_server_configs_stored_in_properties() const = 0;

  // Sets the number of server configs (QuicServerInfo objects) to be persisted.
  virtual void SetMaxServerConfigsStoredInProperties(
      size_t max_server_configs_stored_in_properties) = 0;

  // Returns whether HttpServerProperties is initialized.
  virtual bool IsInitialized() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerProperties);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_H_
