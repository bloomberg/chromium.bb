// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/port_util.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_server_properties.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_hostname_utils.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// "version" 0 indicates, http_server_properties doesn't have "version"
// property.
const int kMissingVersion = 0;

// The version number of persisted http_server_properties.
const int kVersionNumber = 5;

// Persist at most 200 currently-broken alternative services to disk.
const int kMaxBrokenAlternativeServicesToPersist = 200;

const char kVersionKey[] = "version";
const char kServersKey[] = "servers";
const char kSupportsSpdyKey[] = "supports_spdy";
const char kSupportsQuicKey[] = "supports_quic";
const char kQuicServers[] = "quic_servers";
const char kServerInfoKey[] = "server_info";
const char kUsedQuicKey[] = "used_quic";
const char kAddressKey[] = "address";
const char kAlternativeServiceKey[] = "alternative_service";
const char kProtocolKey[] = "protocol_str";
const char kHostKey[] = "host";
const char kPortKey[] = "port";
const char kExpirationKey[] = "expiration";
const char kAdvertisedVersionsKey[] = "advertised_versions";
const char kNetworkStatsKey[] = "network_stats";
const char kSrttKey[] = "srtt";
const char kBrokenAlternativeServicesKey[] = "broken_alternative_services";
const char kBrokenUntilKey[] = "broken_until";
const char kBrokenCountKey[] = "broken_count";

void AddAlternativeServiceFieldsToDictionaryValue(
    const AlternativeService& alternative_service,
    base::DictionaryValue* dict) {
  dict->SetInteger(kPortKey, alternative_service.port);
  if (!alternative_service.host.empty()) {
    dict->SetString(kHostKey, alternative_service.host);
  }
  dict->SetString(kProtocolKey,
                  NextProtoToString(alternative_service.protocol));
}

// A local or temporary data structure to hold preferences for a server.
// This is used only in UpdatePrefs.
//
// TODO(mmenke): Replace this with HttpServerProperties::ServerInfo, once it
// contains all the relevant data.
struct ServerPref {
  bool supports_spdy = false;
  AlternativeServiceInfoVector alternative_service_info_vector;
  bool server_network_stats_valid = false;
  ServerNetworkStats server_network_stats;
};

quic::QuicServerId QuicServerIdFromString(const std::string& str) {
  GURL url(str);
  if (!url.is_valid()) {
    return quic::QuicServerId();
  }
  HostPortPair host_port_pair = HostPortPair::FromURL(url);
  return quic::QuicServerId(host_port_pair.host(), host_port_pair.port(),
                            url.path_piece() == "/private"
                                ? PRIVACY_MODE_ENABLED
                                : PRIVACY_MODE_DISABLED);
}

std::string QuicServerIdToString(const quic::QuicServerId& server_id) {
  HostPortPair host_port_pair(server_id.host(), server_id.port());
  return "https://" + host_port_pair.ToString() +
         (server_id.privacy_mode_enabled() ? "/private" : "");
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  HttpServerPropertiesManager

HttpServerPropertiesManager::HttpServerPropertiesManager(
    std::unique_ptr<HttpServerProperties::PrefDelegate> pref_delegate,
    OnPrefsLoadedCallback on_prefs_loaded_callback,
    size_t max_server_configs_stored_in_properties,
    NetLog* net_log,
    const base::TickClock* clock)
    : pref_delegate_(std::move(pref_delegate)),
      on_prefs_loaded_callback_(std::move(on_prefs_loaded_callback)),
      max_server_configs_stored_in_properties_(
          max_server_configs_stored_in_properties),
      clock_(clock),
      net_log_(
          NetLogWithSource::Make(net_log,
                                 NetLogSourceType::HTTP_SERVER_PROPERTIES)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_delegate_);
  DCHECK(on_prefs_loaded_callback_);
  DCHECK(clock_);

  pref_delegate_->WaitForPrefLoad(
      base::BindOnce(&HttpServerPropertiesManager::OnHttpServerPropertiesLoaded,
                     pref_load_weak_ptr_factory_.GetWeakPtr()));
  net_log_.BeginEvent(NetLogEventType::HTTP_SERVER_PROPERTIES_INITIALIZATION);
}

HttpServerPropertiesManager::~HttpServerPropertiesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HttpServerPropertiesManager::ReadPrefs(
    std::unique_ptr<HttpServerProperties::ServerInfoMap>* server_info_map,
    std::unique_ptr<AlternativeServiceMap>* alternative_service_map,
    std::unique_ptr<ServerNetworkStatsMap>* server_network_stats_map,
    IPAddress* last_quic_address,
    std::unique_ptr<QuicServerInfoMap>* quic_server_info_map,
    std::unique_ptr<BrokenAlternativeServiceList>*
        broken_alternative_service_list,
    std::unique_ptr<RecentlyBrokenAlternativeServices>*
        recently_broken_alternative_services) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  net_log_.EndEvent(NetLogEventType::HTTP_SERVER_PROPERTIES_INITIALIZATION);

  const base::DictionaryValue* http_server_properties_dict =
      pref_delegate_->GetServerProperties();
  // If there are no preferences set, do nothing.
  if (!http_server_properties_dict)
    return;

  net_log_.AddEvent(NetLogEventType::HTTP_SERVER_PROPERTIES_UPDATE_CACHE,
                    [&] { return http_server_properties_dict->Clone(); });
  int version_number = kMissingVersion;
  if (!http_server_properties_dict->GetIntegerWithoutPathExpansion(
          kVersionKey, &version_number) ||
      version_number != kVersionNumber) {
    DVLOG(1) << "Missing or unsupported. Clearing all properties. "
             << version_number;
    return;
  }

  const base::DictionaryValue* servers_dict = nullptr;
  const base::ListValue* servers_list = nullptr;
  // For Version 5, data is stored in the following format.
  // |servers| are saved in MRU order. |servers| are in the format flattened
  // representation of (scheme/host/port) where port might be ignored if is
  // default with scheme.
  //
  // "http_server_properties": {
  //      "servers": [
  //          {"https://yt3.ggpht.com" : {...}},
  //          {"http://0.client-channel.google.com:443" : {...}},
  //          {"http://0-edge-chat.facebook.com" : {...}},
  //          ...
  //      ], ...
  // },
  if (!http_server_properties_dict->GetListWithoutPathExpansion(
          kServersKey, &servers_list)) {
    DVLOG(1) << "Malformed http_server_properties for servers list.";
    return;
  }

  ReadSupportsQuic(*http_server_properties_dict, last_quic_address);

  *server_info_map = std::make_unique<HttpServerProperties::ServerInfoMap>();
  *alternative_service_map = std::make_unique<AlternativeServiceMap>();
  *server_network_stats_map = std::make_unique<ServerNetworkStatsMap>();
  *quic_server_info_map = std::make_unique<QuicServerInfoMap>(
      max_server_configs_stored_in_properties_);

  // Iterate servers list in reverse MRU order so that entries are inserted
  // into |spdy_servers_map|, |alternative_service_map|, and
  // |server_network_stats_map| from oldest to newest.
  for (auto it = servers_list->end(); it != servers_list->begin();) {
    --it;
    if (!it->GetAsDictionary(&servers_dict)) {
      DVLOG(1) << "Malformed http_server_properties for servers dictionary.";
      continue;
    }
    AddServersData(*servers_dict, server_info_map->get(),
                   alternative_service_map->get(),
                   server_network_stats_map->get());
  }

  AddToQuicServerInfoMap(*http_server_properties_dict,
                         quic_server_info_map->get());

  // Read list containing broken and recently-broken alternative services, if
  // it exists.
  const base::ListValue* broken_alt_svc_list;
  if (http_server_properties_dict->GetListWithoutPathExpansion(
          kBrokenAlternativeServicesKey, &broken_alt_svc_list)) {
    *broken_alternative_service_list =
        std::make_unique<BrokenAlternativeServiceList>();
    *recently_broken_alternative_services =
        std::make_unique<RecentlyBrokenAlternativeServices>(
            kMaxRecentlyBrokenAlternativeServiceEntries);

    // Iterate list in reverse-MRU order
    for (auto it = broken_alt_svc_list->end();
         it != broken_alt_svc_list->begin();) {
      --it;
      const base::DictionaryValue* entry_dict;
      if (!it->GetAsDictionary(&entry_dict)) {
        DVLOG(1) << "Malformed broken alterantive service entry.";
        continue;
      }
      AddToBrokenAlternativeServices(
          *entry_dict, broken_alternative_service_list->get(),
          recently_broken_alternative_services->get());
    }
  }

  // Set the properties loaded from prefs on |http_server_properties_impl_|.

  // TODO(mmenke): Rename this once more information is stored in this map.
  UMA_HISTOGRAM_COUNTS_1M("Net.HttpServerProperties.CountOfServers",
                          (*server_info_map)->size());

  // Update the cached data and use the new alternative service list from
  // preferences.
  UMA_HISTOGRAM_COUNTS_1M("Net.CountOfAlternateProtocolServers",
                          (*alternative_service_map)->size());

  UMA_HISTOGRAM_COUNTS_1000("Net.CountOfQuicServerInfos",
                            (*quic_server_info_map)->size());

  if (*recently_broken_alternative_services) {
    DCHECK(*broken_alternative_service_list);

    UMA_HISTOGRAM_COUNTS_1000("Net.CountOfBrokenAlternativeServices",
                              (*broken_alternative_service_list)->size());
    UMA_HISTOGRAM_COUNTS_1000("Net.CountOfRecentlyBrokenAlternativeServices",
                              (*recently_broken_alternative_services)->size());
  }
}

void HttpServerPropertiesManager::AddToBrokenAlternativeServices(
    const base::DictionaryValue& broken_alt_svc_entry_dict,
    BrokenAlternativeServiceList* broken_alternative_service_list,
    RecentlyBrokenAlternativeServices* recently_broken_alternative_services) {
  AlternativeService alt_service;
  if (!ParseAlternativeServiceDict(broken_alt_svc_entry_dict, false,
                                   "broken alternative services",
                                   &alt_service)) {
    return;
  }

  // Each entry must contain either broken-count and/or broken-until fields.
  bool contains_broken_count_or_broken_until = false;

  // Read broken-count and add an entry for |alt_service| into
  // |recently_broken_alternative_services|.
  if (broken_alt_svc_entry_dict.HasKey(kBrokenCountKey)) {
    int broken_count;
    if (!broken_alt_svc_entry_dict.GetIntegerWithoutPathExpansion(
            kBrokenCountKey, &broken_count)) {
      DVLOG(1) << "Recently broken alternative service has malformed "
               << "broken-count.";
      return;
    }
    if (broken_count < 0) {
      DVLOG(1) << "Broken alternative service has negative broken-count.";
      return;
    }
    recently_broken_alternative_services->Put(alt_service, broken_count);
    contains_broken_count_or_broken_until = true;
  }

  // Read broken-until and add an entry for |alt_service| in
  // |broken_alternative_service_list|.
  if (broken_alt_svc_entry_dict.HasKey(kBrokenUntilKey)) {
    std::string expiration_string;
    int64_t expiration_int64;
    if (!broken_alt_svc_entry_dict.GetStringWithoutPathExpansion(
            kBrokenUntilKey, &expiration_string) ||
        !base::StringToInt64(expiration_string, &expiration_int64)) {
      DVLOG(1) << "Broken alternative service has malformed broken-until "
               << "string.";
      return;
    }

    time_t expiration_time_t = static_cast<time_t>(expiration_int64);
    // Convert expiration from time_t to Time to TimeTicks
    base::TimeTicks expiration_time_ticks =
        clock_->NowTicks() +
        (base::Time::FromTimeT(expiration_time_t) - base::Time::Now());
    broken_alternative_service_list->push_back(
        std::make_pair(alt_service, expiration_time_ticks));
    contains_broken_count_or_broken_until = true;
  }

  if (!contains_broken_count_or_broken_until) {
    DVLOG(1) << "Broken alternative service has neither broken-count nor "
             << "broken-until specified.";
  }
}

void HttpServerPropertiesManager::AddServersData(
    const base::DictionaryValue& servers_dict,
    HttpServerProperties::ServerInfoMap* server_info_map,
    AlternativeServiceMap* alternative_service_map,
    ServerNetworkStatsMap* network_stats_map) {
  for (base::DictionaryValue::Iterator it(servers_dict); !it.IsAtEnd();
       it.Advance()) {
    // Get server's scheme/host/pair.
    const std::string& server_str = it.key();
    std::string spdy_server_url = server_str;
    url::SchemeHostPort spdy_server((GURL(spdy_server_url)));
    if (spdy_server.host().empty()) {
      DVLOG(1) << "Malformed http_server_properties for server: " << server_str;
      return;
    }

    const base::DictionaryValue* server_pref_dict = nullptr;
    if (!it.value().GetAsDictionary(&server_pref_dict)) {
      DVLOG(1) << "Malformed http_server_properties server: " << server_str;
      return;
    }

    // Get if server supports Spdy.
    bool supports_spdy;
    if (server_pref_dict->GetBoolean(kSupportsSpdyKey, &supports_spdy)) {
      HttpServerProperties::ServerInfo server_info;
      server_info.supports_spdy = supports_spdy;
      server_info_map->Put(spdy_server, std::move(server_info));
    }

    if (AddToAlternativeServiceMap(spdy_server, *server_pref_dict,
                                   alternative_service_map)) {
      AddToNetworkStatsMap(spdy_server, *server_pref_dict, network_stats_map);
    }
  }
}

bool HttpServerPropertiesManager::ParseAlternativeServiceDict(
    const base::DictionaryValue& dict,
    bool host_optional,
    const std::string& parsing_under,
    AlternativeService* alternative_service) {
  // Protocol is mandatory.
  std::string protocol_str;
  if (!dict.GetStringWithoutPathExpansion(kProtocolKey, &protocol_str)) {
    DVLOG(1) << "Malformed alternative service protocol string under: "
             << parsing_under;
    return false;
  }
  NextProto protocol = NextProtoFromString(protocol_str);
  if (!IsAlternateProtocolValid(protocol)) {
    DVLOG(1) << "Invalid alternative service protocol string \"" << protocol_str
             << "\" under: " << parsing_under;
    return false;
  }
  alternative_service->protocol = protocol;

  // If host is optional, it defaults to "".
  std::string host = "";
  if (dict.HasKey(kHostKey)) {
    if (!dict.GetStringWithoutPathExpansion(kHostKey, &host)) {
      DVLOG(1) << "Malformed alternative service host string under: "
               << parsing_under;
      return false;
    }
  } else if (!host_optional) {
    DVLOG(1) << "alternative service missing host string under: "
             << parsing_under;
    return false;
  }
  alternative_service->host = host;

  // Port is mandatory.
  int port = 0;
  if (!dict.GetInteger(kPortKey, &port) || !IsPortValid(port)) {
    DVLOG(1) << "Malformed alternative service port under: " << parsing_under;
    return false;
  }
  alternative_service->port = static_cast<uint32_t>(port);

  return true;
}

bool HttpServerPropertiesManager::ParseAlternativeServiceInfoDictOfServer(
    const base::DictionaryValue& dict,
    const std::string& server_str,
    AlternativeServiceInfo* alternative_service_info) {
  AlternativeService alternative_service;
  if (!ParseAlternativeServiceDict(dict, true, "server " + server_str,
                                   &alternative_service)) {
    return false;
  }
  alternative_service_info->set_alternative_service(alternative_service);

  // Expiration is optional, defaults to one day.
  if (!dict.HasKey(kExpirationKey)) {
    alternative_service_info->set_expiration(base::Time::Now() +
                                             base::TimeDelta::FromDays(1));
  } else {
    std::string expiration_string;
    if (dict.GetStringWithoutPathExpansion(kExpirationKey,
                                           &expiration_string)) {
      int64_t expiration_int64 = 0;
      if (!base::StringToInt64(expiration_string, &expiration_int64)) {
        DVLOG(1) << "Malformed alternative service expiration for server: "
                 << server_str;
        return false;
      }
      alternative_service_info->set_expiration(
          base::Time::FromInternalValue(expiration_int64));
    } else {
      DVLOG(1) << "Malformed alternative service expiration for server: "
               << server_str;
      return false;
    }
  }

  // Advertised versions list is optional.
  if (dict.HasKey(kAdvertisedVersionsKey)) {
    const base::ListValue* versions_list = nullptr;
    if (!dict.GetListWithoutPathExpansion(kAdvertisedVersionsKey,
                                          &versions_list)) {
      DVLOG(1) << "Malformed alternative service advertised versions list for "
               << "server: " << server_str;
      return false;
    }
    quic::ParsedQuicVersionVector advertised_versions;
    for (const auto& value : *versions_list) {
      int version;
      if (!value.GetAsInteger(&version)) {
        DVLOG(1) << "Malformed alternative service version for server: "
                 << server_str;
        return false;
      }
      // TODO(nharper): Support ParsedQuicVersions (instead of
      // QuicTransportVersions) in AlternativeServiceMap.
      advertised_versions.push_back(quic::ParsedQuicVersion(
          quic::PROTOCOL_QUIC_CRYPTO, quic::QuicTransportVersion(version)));
    }
    alternative_service_info->set_advertised_versions(advertised_versions);
  }

  return true;
}

bool HttpServerPropertiesManager::AddToAlternativeServiceMap(
    const url::SchemeHostPort& server,
    const base::DictionaryValue& server_pref_dict,
    AlternativeServiceMap* alternative_service_map) {
  DCHECK(alternative_service_map->Peek(server) ==
         alternative_service_map->end());
  const base::ListValue* alternative_service_list;
  if (!server_pref_dict.GetListWithoutPathExpansion(
          kAlternativeServiceKey, &alternative_service_list)) {
    return true;
  }
  if (server.scheme() != "https") {
    return false;
  }

  AlternativeServiceInfoVector alternative_service_info_vector;
  for (const auto& alternative_service_list_item : *alternative_service_list) {
    const base::DictionaryValue* alternative_service_dict;
    if (!alternative_service_list_item.GetAsDictionary(
            &alternative_service_dict))
      return false;
    AlternativeServiceInfo alternative_service_info;
    if (!ParseAlternativeServiceInfoDictOfServer(*alternative_service_dict,
                                                 server.Serialize(),
                                                 &alternative_service_info)) {
      return false;
    }
    if (base::Time::Now() < alternative_service_info.expiration()) {
      alternative_service_info_vector.push_back(alternative_service_info);
    }
  }

  if (alternative_service_info_vector.empty()) {
    return false;
  }

  alternative_service_map->Put(server, alternative_service_info_vector);
  return true;
}

void HttpServerPropertiesManager::ReadSupportsQuic(
    const base::DictionaryValue& http_server_properties_dict,
    IPAddress* last_quic_address) {
  const base::DictionaryValue* supports_quic_dict = nullptr;
  if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
          kSupportsQuicKey, &supports_quic_dict)) {
    return;
  }
  bool used_quic = false;
  if (!supports_quic_dict->GetBooleanWithoutPathExpansion(kUsedQuicKey,
                                                          &used_quic)) {
    DVLOG(1) << "Malformed SupportsQuic";
    return;
  }
  if (!used_quic)
    return;

  std::string address;
  if (!supports_quic_dict->GetStringWithoutPathExpansion(kAddressKey,
                                                         &address) ||
      !last_quic_address->AssignFromIPLiteral(address)) {
    DVLOG(1) << "Malformed SupportsQuic";
  }
}

void HttpServerPropertiesManager::AddToNetworkStatsMap(
    const url::SchemeHostPort& server,
    const base::DictionaryValue& server_pref_dict,
    ServerNetworkStatsMap* network_stats_map) {
  DCHECK(network_stats_map->Peek(server) == network_stats_map->end());
  const base::DictionaryValue* server_network_stats_dict = nullptr;
  if (!server_pref_dict.GetDictionaryWithoutPathExpansion(
          kNetworkStatsKey, &server_network_stats_dict)) {
    return;
  }
  int srtt;
  if (!server_network_stats_dict->GetIntegerWithoutPathExpansion(kSrttKey,
                                                                 &srtt)) {
    DVLOG(1) << "Malformed ServerNetworkStats for server: "
             << server.Serialize();
    return;
  }
  ServerNetworkStats server_network_stats;
  server_network_stats.srtt = base::TimeDelta::FromMicroseconds(srtt);
  // TODO(rtenneti): When QUIC starts using bandwidth_estimate, then persist
  // bandwidth_estimate.
  network_stats_map->Put(server, server_network_stats);
}

void HttpServerPropertiesManager::AddToQuicServerInfoMap(
    const base::DictionaryValue& http_server_properties_dict,
    QuicServerInfoMap* quic_server_info_map) {
  const base::DictionaryValue* quic_servers_dict = nullptr;
  if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
          kQuicServers, &quic_servers_dict)) {
    DVLOG(1) << "Malformed http_server_properties for quic_servers.";
    return;
  }

  for (base::DictionaryValue::Iterator it(*quic_servers_dict); !it.IsAtEnd();
       it.Advance()) {
    // Get quic_server_id.
    const std::string& quic_server_id_str = it.key();

    quic::QuicServerId quic_server_id =
        QuicServerIdFromString(quic_server_id_str);
    if (quic_server_id.host().empty()) {
      DVLOG(1) << "Malformed http_server_properties for quic server: "
               << quic_server_id_str;
      continue;
    }

    const base::DictionaryValue* quic_server_pref_dict = nullptr;
    if (!it.value().GetAsDictionary(&quic_server_pref_dict)) {
      DVLOG(1) << "Malformed http_server_properties quic server dict: "
               << quic_server_id_str;
      continue;
    }

    std::string quic_server_info;
    if (!quic_server_pref_dict->GetStringWithoutPathExpansion(
            kServerInfoKey, &quic_server_info)) {
      DVLOG(1) << "Malformed http_server_properties quic server info: "
               << quic_server_id_str;
      continue;
    }
    quic_server_info_map->Put(quic_server_id, quic_server_info);
  }
}

void HttpServerPropertiesManager::WriteToPrefs(
    const HttpServerProperties::ServerInfoMap& server_info_map,
    const AlternativeServiceMap& alternative_service_map,
    const GetCannonicalSuffix& get_canonical_suffix,
    const ServerNetworkStatsMap& server_network_stats_map,
    const IPAddress& last_quic_address,
    const QuicServerInfoMap& quic_server_info_map,
    const BrokenAlternativeServiceList& broken_alternative_service_list,
    const RecentlyBrokenAlternativeServices&
        recently_broken_alternative_services,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If loading prefs hasn't completed, don't call it, since this will overwrite
  // existing prefs.
  on_prefs_loaded_callback_.Reset();

  typedef base::MRUCache<url::SchemeHostPort, ServerPref> ServerPrefMap;
  ServerPrefMap server_pref_map(ServerPrefMap::NO_AUTO_EVICT);

  // Add SPDY servers to |server_pref_map|.
  for (auto it = server_info_map.rbegin(); it != server_info_map.rend(); ++it) {
    // Only add servers that support SPDY.
    if (!it->second.supports_spdy.value_or(false))
      continue;

    auto map_it = server_pref_map.Put(it->first, ServerPref());
    map_it->second.supports_spdy = true;
  }

  // Add alternative service info to |server_pref_map|.
  UMA_HISTOGRAM_COUNTS_1M("Net.CountOfAlternateProtocolServers.Memory",
                          alternative_service_map.size());
  typedef std::map<std::string, bool> CanonicalHostPersistedMap;
  CanonicalHostPersistedMap persisted_map;
  const base::Time now = base::Time::Now();
  for (auto it = alternative_service_map.rbegin();
       it != alternative_service_map.rend(); ++it) {
    const url::SchemeHostPort& server = it->first;
    AlternativeServiceInfoVector notbroken_alternative_service_info_vector;
    for (const AlternativeServiceInfo& alternative_service_info : it->second) {
      // Do not persist expired entries
      if (alternative_service_info.expiration() < now) {
        continue;
      }
      if (!IsAlternateProtocolValid(
              alternative_service_info.alternative_service().protocol)) {
        continue;
      }
      notbroken_alternative_service_info_vector.push_back(
          alternative_service_info);
    }
    if (notbroken_alternative_service_info_vector.empty()) {
      continue;
    }
    const std::string* canonical_suffix =
        get_canonical_suffix.Run(server.host());
    if (canonical_suffix != nullptr) {
      if (persisted_map.find(*canonical_suffix) != persisted_map.end())
        continue;
      persisted_map[*canonical_suffix] = true;
    }

    auto map_it = server_pref_map.Get(server);
    if (map_it == server_pref_map.end())
      map_it = server_pref_map.Put(server, ServerPref());
    map_it->second.alternative_service_info_vector =
        std::move(notbroken_alternative_service_info_vector);
  }

  // Add server network stats to |server_pref_map|.
  for (auto it = server_network_stats_map.rbegin();
       it != server_network_stats_map.rend(); ++it) {
    const url::SchemeHostPort& server = it->first;
    auto map_it = server_pref_map.Get(server);
    if (map_it == server_pref_map.end())
      map_it = server_pref_map.Put(server, ServerPref());
    map_it->second.server_network_stats_valid = true;
    map_it->second.server_network_stats = it->second;
  }

  base::DictionaryValue http_server_properties_dict;

  // Convert |server_pref_map| to a DictionaryValue and add it to
  // |http_server_properties_dict|.
  auto servers_list = std::make_unique<base::ListValue>();
  for (ServerPrefMap::const_reverse_iterator map_it = server_pref_map.rbegin();
       map_it != server_pref_map.rend(); ++map_it) {
    const url::SchemeHostPort server = map_it->first;
    const ServerPref& server_pref = map_it->second;

    auto servers_dict = std::make_unique<base::DictionaryValue>();
    auto server_pref_dict = std::make_unique<base::DictionaryValue>();

    if (server_pref.supports_spdy) {
      server_pref_dict->SetBoolean(kSupportsSpdyKey, server_pref.supports_spdy);
    }
    if (!server_pref.alternative_service_info_vector.empty()) {
      SaveAlternativeServiceToServerPrefs(
          server_pref.alternative_service_info_vector, server_pref_dict.get());
    }
    if (server_pref.server_network_stats_valid) {
      SaveNetworkStatsToServerPrefs(server_pref.server_network_stats,
                                    server_pref_dict.get());
    }

    servers_dict->SetWithoutPathExpansion(server.Serialize(),
                                          std::move(server_pref_dict));
    bool value = servers_list->AppendIfNotPresent(std::move(servers_dict));
    DCHECK(value);  // Should never happen.
  }
  http_server_properties_dict.SetWithoutPathExpansion(kServersKey,
                                                      std::move(servers_list));

  http_server_properties_dict.SetInteger(kVersionKey, kVersionNumber);

  if (last_quic_address.IsValid())
    SaveSupportsQuicToPrefs(last_quic_address, &http_server_properties_dict);

  SaveQuicServerInfoMapToServerPrefs(quic_server_info_map,
                                     &http_server_properties_dict);

  SaveBrokenAlternativeServicesToPrefs(
      broken_alternative_service_list, kMaxBrokenAlternativeServicesToPersist,
      recently_broken_alternative_services, &http_server_properties_dict);

  pref_delegate_->SetServerProperties(http_server_properties_dict,
                                      std::move(callback));

  net_log_.AddEvent(NetLogEventType::HTTP_SERVER_PROPERTIES_UPDATE_PREFS,
                    [&] { return http_server_properties_dict.Clone(); });
}

void HttpServerPropertiesManager::SaveAlternativeServiceToServerPrefs(
    const AlternativeServiceInfoVector& alternative_service_info_vector,
    base::DictionaryValue* server_pref_dict) {
  if (alternative_service_info_vector.empty()) {
    return;
  }
  std::unique_ptr<base::ListValue> alternative_service_list(
      new base::ListValue);
  for (const AlternativeServiceInfo& alternative_service_info :
       alternative_service_info_vector) {
    const AlternativeService& alternative_service =
        alternative_service_info.alternative_service();
    DCHECK(IsAlternateProtocolValid(alternative_service.protocol));
    std::unique_ptr<base::DictionaryValue> alternative_service_dict(
        new base::DictionaryValue);
    AddAlternativeServiceFieldsToDictionaryValue(
        alternative_service, alternative_service_dict.get());
    // JSON cannot store int64_t, so expiration is converted to a string.
    alternative_service_dict->SetString(
        kExpirationKey,
        base::NumberToString(
            alternative_service_info.expiration().ToInternalValue()));
    std::unique_ptr<base::ListValue> advertised_versions_list =
        std::make_unique<base::ListValue>();
    for (const auto& version : alternative_service_info.advertised_versions()) {
      advertised_versions_list->AppendInteger(version.transport_version);
    }
    alternative_service_dict->SetList(kAdvertisedVersionsKey,
                                      std::move(advertised_versions_list));
    alternative_service_list->Append(std::move(alternative_service_dict));
  }
  if (alternative_service_list->GetSize() == 0)
    return;
  server_pref_dict->SetWithoutPathExpansion(
      kAlternativeServiceKey, std::move(alternative_service_list));
}

void HttpServerPropertiesManager::SaveSupportsQuicToPrefs(
    const IPAddress& last_quic_address,
    base::DictionaryValue* http_server_properties_dict) {
  if (!last_quic_address.IsValid())
    return;

  auto supports_quic_dict = std::make_unique<base::DictionaryValue>();
  supports_quic_dict->SetBoolean(kUsedQuicKey, true);
  supports_quic_dict->SetString(kAddressKey, last_quic_address.ToString());
  http_server_properties_dict->SetWithoutPathExpansion(
      kSupportsQuicKey, std::move(supports_quic_dict));
}

void HttpServerPropertiesManager::SaveNetworkStatsToServerPrefs(
    const ServerNetworkStats& server_network_stats,
    base::DictionaryValue* server_pref_dict) {
  auto server_network_stats_dict = std::make_unique<base::DictionaryValue>();
  // Becasue JSON doesn't support int64_t, persist int64_t as a string.
  server_network_stats_dict->SetInteger(
      kSrttKey, static_cast<int>(server_network_stats.srtt.InMicroseconds()));
  // TODO(rtenneti): When QUIC starts using bandwidth_estimate, then persist
  // bandwidth_estimate.
  server_pref_dict->SetWithoutPathExpansion(
      kNetworkStatsKey, std::move(server_network_stats_dict));
}

void HttpServerPropertiesManager::SaveQuicServerInfoMapToServerPrefs(
    const QuicServerInfoMap& quic_server_info_map,
    base::DictionaryValue* http_server_properties_dict) {
  if (quic_server_info_map.empty())
    return;
  auto quic_servers_dict = std::make_unique<base::DictionaryValue>();
  for (auto it = quic_server_info_map.rbegin();
       it != quic_server_info_map.rend(); ++it) {
    const quic::QuicServerId& server_id = it->first;
    auto quic_server_pref_dict = std::make_unique<base::DictionaryValue>();
    quic_server_pref_dict->SetKey(kServerInfoKey, base::Value(it->second));
    quic_servers_dict->SetWithoutPathExpansion(
        QuicServerIdToString(server_id), std::move(quic_server_pref_dict));
  }
  http_server_properties_dict->SetWithoutPathExpansion(
      kQuicServers, std::move(quic_servers_dict));
}

void HttpServerPropertiesManager::SaveBrokenAlternativeServicesToPrefs(
    const BrokenAlternativeServiceList& broken_alternative_service_list,
    size_t max_broken_alternative_services,
    const RecentlyBrokenAlternativeServices&
        recently_broken_alternative_services,
    base::DictionaryValue* http_server_properties_dict) {
  if (broken_alternative_service_list.empty() &&
      recently_broken_alternative_services.empty())
    return;

  // JSON list will be in MRU order according to
  // |recently_broken_alternative_services|.
  std::unique_ptr<base::ListValue> json_list =
      std::make_unique<base::ListValue>();

  // Maps recently-broken alternative services to the index where it's stored
  // in |json_list|.
  std::unordered_map<AlternativeService, size_t, AlternativeServiceHash>
      json_list_index_map;

  if (!recently_broken_alternative_services.empty()) {
    for (auto it = recently_broken_alternative_services.rbegin();
         it != recently_broken_alternative_services.rend(); ++it) {
      const AlternativeService& alt_service = it->first;
      int broken_count = it->second;
      base::DictionaryValue entry_dict;
      AddAlternativeServiceFieldsToDictionaryValue(alt_service, &entry_dict);
      entry_dict.SetKey(kBrokenCountKey, base::Value(broken_count));
      json_list_index_map[alt_service] = json_list->GetList().size();
      json_list->GetList().push_back(std::move(entry_dict));
    }
  }

  if (!broken_alternative_service_list.empty()) {
    // Add expiration time info from |broken_alternative_service_list| to
    // the JSON list.
    size_t count = 0;
    for (auto it = broken_alternative_service_list.begin();
         it != broken_alternative_service_list.end() &&
         count < max_broken_alternative_services;
         ++it, ++count) {
      const AlternativeService& alt_service = it->first;
      base::TimeTicks expiration_time_ticks = it->second;
      // Convert expiration from TimeTicks to Time to time_t
      time_t expiration_time_t =
          (base::Time::Now() + (expiration_time_ticks - clock_->NowTicks()))
              .ToTimeT();
      int64_t expiration_int64 = static_cast<int64_t>(expiration_time_t);

      auto index_map_it = json_list_index_map.find(alt_service);
      if (index_map_it != json_list_index_map.end()) {
        size_t json_list_index = index_map_it->second;
        base::DictionaryValue* entry_dict = nullptr;
        bool result = json_list->GetDictionary(json_list_index, &entry_dict);
        DCHECK(result);
        DCHECK(!entry_dict->HasKey(kBrokenUntilKey));
        entry_dict->SetKey(kBrokenUntilKey,
                           base::Value(base::NumberToString(expiration_int64)));
      } else {
        base::DictionaryValue entry_dict;
        AddAlternativeServiceFieldsToDictionaryValue(alt_service, &entry_dict);
        entry_dict.SetKey(kBrokenUntilKey,
                          base::Value(base::NumberToString(expiration_int64)));
        json_list->GetList().push_back(std::move(entry_dict));
      }
    }
  }

  DCHECK(!json_list->empty());
  http_server_properties_dict->SetWithoutPathExpansion(
      kBrokenAlternativeServicesKey, std::move(json_list));
}

void HttpServerPropertiesManager::OnHttpServerPropertiesLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If prefs have already been written, nothing to do.
  if (!on_prefs_loaded_callback_)
    return;

  std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map;
  std::unique_ptr<AlternativeServiceMap> alternative_service_map;
  std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map;
  IPAddress last_quic_address;
  std::unique_ptr<QuicServerInfoMap> quic_server_info_map;
  std::unique_ptr<BrokenAlternativeServiceList> broken_alternative_service_list;
  std::unique_ptr<RecentlyBrokenAlternativeServices>
      recently_broken_alternative_services;

  ReadPrefs(&server_info_map, &alternative_service_map,
            &server_network_stats_map, &last_quic_address,
            &quic_server_info_map, &broken_alternative_service_list,
            &recently_broken_alternative_services);

  std::move(on_prefs_loaded_callback_)
      .Run(std::move(server_info_map), std::move(alternative_service_map),
           std::move(server_network_stats_map), last_quic_address,
           std::move(quic_server_info_map),
           std::move(broken_alternative_service_list),
           std::move(recently_broken_alternative_services));
}

}  // namespace net
