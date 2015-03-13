// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "base/containers/mru_cache.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/net_util.h"
#include "net/quic/quic_bandwidth.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_framer.h"  // TODO(willchan): Reconsider this.
#include "net/spdy/spdy_protocol.h"

namespace net {

struct SSLConfig;

enum AlternateProtocolUsage {
  // Alternate Protocol was used without racing a normal connection.
  ALTERNATE_PROTOCOL_USAGE_NO_RACE = 0,
  // Alternate Protocol was used by winning a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_WON_RACE = 1,
  // Alternate Protocol was not used by losing a race with a normal connection.
  ALTERNATE_PROTOCOL_USAGE_LOST_RACE = 2,
  // Alternate Protocol was not used because no Alternate-Protocol information
  // was available when the request was issued, but an Alternate-Protocol header
  // was present in the response.
  ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING = 3,
  // Alternate Protocol was not used because it was marked broken.
  ALTERNATE_PROTOCOL_USAGE_BROKEN = 4,
  // Maximum value for the enum.
  ALTERNATE_PROTOCOL_USAGE_MAX,
};

// Log a histogram to reflect |usage|.
NET_EXPORT void HistogramAlternateProtocolUsage(AlternateProtocolUsage usage);

enum BrokenAlternateProtocolLocation {
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB = 0,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_QUIC_STREAM_FACTORY = 1,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB_ALT = 2,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_HTTP_STREAM_FACTORY_IMPL_JOB_MAIN = 3,
  BROKEN_ALTERNATE_PROTOCOL_LOCATION_MAX,
};

// Log a histogram to reflect |location|.
NET_EXPORT void HistogramBrokenAlternateProtocolLocation(
    BrokenAlternateProtocolLocation location);

enum AlternateProtocol {
  DEPRECATED_NPN_SPDY_2 = 0,
  ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION = DEPRECATED_NPN_SPDY_2,
  NPN_SPDY_MINIMUM_VERSION = DEPRECATED_NPN_SPDY_2,
  NPN_SPDY_3,
  NPN_SPDY_3_1,
  NPN_SPDY_4_14,  // HTTP/2 draft-14
  NPN_SPDY_4,     // HTTP/2
  NPN_SPDY_MAXIMUM_VERSION = NPN_SPDY_4,
  QUIC,
  ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION = QUIC,
  UNINITIALIZED_ALTERNATE_PROTOCOL,
};

// Simply returns whether |protocol| is between
// ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION and
// ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION (inclusive).
NET_EXPORT bool IsAlternateProtocolValid(AlternateProtocol protocol);

enum AlternateProtocolSize {
  NUM_VALID_ALTERNATE_PROTOCOLS =
    ALTERNATE_PROTOCOL_MAXIMUM_VALID_VERSION -
    ALTERNATE_PROTOCOL_MINIMUM_VALID_VERSION + 1,
};

NET_EXPORT const char* AlternateProtocolToString(AlternateProtocol protocol);
NET_EXPORT AlternateProtocol AlternateProtocolFromString(
    const std::string& str);
NET_EXPORT_PRIVATE AlternateProtocol AlternateProtocolFromNextProto(
    NextProto next_proto);

struct NET_EXPORT AlternativeService {
  AlternativeService()
      : protocol(UNINITIALIZED_ALTERNATE_PROTOCOL), host(), port(0) {}

  AlternativeService(AlternateProtocol protocol,
                     const std::string& host,
                     uint16 port)
      : protocol(protocol), host(host), port(port) {}

  AlternativeService(const AlternativeService& alternative_service) = default;
  AlternativeService& operator=(const AlternativeService& alternative_service) =
      default;

  bool operator==(const AlternativeService& other) const {
    return protocol == other.protocol && host == other.host &&
           port == other.port;
  }

  bool operator<(const AlternativeService& other) const {
    if (protocol != other.protocol)
      return protocol < other.protocol;
    if (host != other.host)
      return host < other.host;
    return port < other.port;
  }

  AlternateProtocol protocol;
  std::string host;
  uint16 port;
};

struct NET_EXPORT AlternateProtocolInfo {
  AlternateProtocolInfo()
      : port(0), protocol(UNINITIALIZED_ALTERNATE_PROTOCOL), probability(0) {}

  AlternateProtocolInfo(uint16 port,
                        AlternateProtocol protocol,
                        double probability)
      : port(port), protocol(protocol), probability(probability) {}

  bool Equals(const AlternateProtocolInfo& other) const {
    return port == other.port &&
        protocol == other.protocol &&
        probability == other.probability;
  }

  std::string ToString() const;

  uint16 port;
  AlternateProtocol protocol;
  double probability;
};

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
  ServerNetworkStats() : bandwidth_estimate(QuicBandwidth::Zero()) {}

  base::TimeDelta srtt;
  QuicBandwidth bandwidth_estimate;
};

typedef base::MRUCache<
    HostPortPair, AlternateProtocolInfo> AlternateProtocolMap;
typedef base::MRUCache<HostPortPair, SettingsMap> SpdySettingsMap;
typedef base::MRUCache<HostPortPair, ServerNetworkStats> ServerNetworkStatsMap;

extern const char kAlternateProtocolHeader[];

// The interface for setting/retrieving the HTTP server properties.
// Currently, this class manages servers':
// * SPDY support (based on NPN results)
// * Alternate-Protocol support
// * Spdy Settings (like CWND ID field)
class NET_EXPORT HttpServerProperties {
 public:
  HttpServerProperties() {}
  virtual ~HttpServerProperties() {}

  // Gets a weak pointer for this object.
  virtual base::WeakPtr<HttpServerProperties> GetWeakPtr() = 0;

  // Deletes all data.
  virtual void Clear() = 0;

  // Returns true if |server| supports a network protocol which honors
  // request prioritization.
  virtual bool SupportsRequestPriority(const HostPortPair& server) = 0;

  // Add |server| into the persistent store. Should only be called from IO
  // thread.
  virtual void SetSupportsSpdy(const HostPortPair& server,
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

  // Returns the AlternateProtocol for |server| if it has probability equal to
  // or exceeding threshold, or else the forced AlternateProtocol if there is
  // one, or else one with UNINITIALIZED_ALTERNATE_PROTOCOL.
  virtual AlternateProtocolInfo GetAlternateProtocol(
      const HostPortPair& server) = 0;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(const HostPortPair& server,
                                    uint16 alternate_port,
                                    AlternateProtocol alternate_protocol,
                                    double probability) = 0;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(const HostPortPair& server) = 0;

  // Returns true iff |alternative_service| is currently broken.
  virtual bool IsAlternativeServiceBroken(
      const AlternativeService& alternative_service) = 0;

  // Returns true if Alternate-Protocol for |server| was recently BROKEN.
  virtual bool WasAlternateProtocolRecentlyBroken(
      const HostPortPair& server) = 0;

  // Confirms that Alternate-Protocol for |server| is working.
  virtual void ConfirmAlternateProtocol(const HostPortPair& server) = 0;

  // Clears the Alternate-Protocol for |server|.
  virtual void ClearAlternateProtocol(const HostPortPair& server) = 0;

  // Returns all Alternate-Protocol mappings.
  virtual const AlternateProtocolMap& alternate_protocol_map() const = 0;

  // Sets the threshold to be used when evaluating Alternate-Protocol
  // advertisments. Only advertisements with a with a probability
  // greater than |threshold| will be honored. |threshold| must be
  // between 0 and 1 inclusive. Hence, a threshold of 0 implies that
  // all advertisements will be honored.
  virtual void SetAlternateProtocolProbabilityThreshold(
      double threshold) = 0;

  // Gets a reference to the SettingsMap stored for a host.
  // If no settings are stored, returns an empty SettingsMap.
  virtual const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) = 0;

  // Saves an individual SPDY setting for a host. Returns true if SPDY setting
  // is to be persisted.
  virtual bool SetSpdySetting(const HostPortPair& host_port_pair,
                              SpdySettingsIds id,
                              SpdySettingsFlags flags,
                              uint32 value) = 0;

  // Clears all SPDY settings for a host.
  virtual void ClearSpdySettings(const HostPortPair& host_port_pair) = 0;

  // Clears all SPDY settings for all hosts.
  virtual void ClearAllSpdySettings() = 0;

  // Returns all persistent SPDY settings.
  virtual const SpdySettingsMap& spdy_settings_map() const = 0;

  virtual bool GetSupportsQuic(IPAddressNumber* last_address) const = 0;

  virtual void SetSupportsQuic(bool used_quic,
                               const IPAddressNumber& last_address) = 0;

  virtual void SetServerNetworkStats(const HostPortPair& host_port_pair,
                                     ServerNetworkStats stats) = 0;

  virtual const ServerNetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) = 0;

  virtual const ServerNetworkStatsMap& server_network_stats_map() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerProperties);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_H_
