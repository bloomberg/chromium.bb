// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"

namespace net {

namespace {

const uint64_t kBrokenAlternativeProtocolDelaySecs = 300;

}  // namespace

HttpServerPropertiesImpl::HttpServerPropertiesImpl()
    : spdy_servers_map_(SpdyServerHostPortMap::NO_AUTO_EVICT),
      alternative_service_map_(AlternativeServiceMap::NO_AUTO_EVICT),
      spdy_settings_map_(SpdySettingsMap::NO_AUTO_EVICT),
      server_network_stats_map_(ServerNetworkStatsMap::NO_AUTO_EVICT),
      alternative_service_probability_threshold_(1.0),
      quic_server_info_map_(QuicServerInfoMap::NO_AUTO_EVICT),
      max_server_configs_stored_in_properties_(kMaxQuicServersToPersist),
      weak_ptr_factory_(this) {
  canonical_suffixes_.push_back(".c.youtube.com");
  canonical_suffixes_.push_back(".googlevideo.com");
  canonical_suffixes_.push_back(".googleusercontent.com");
}

HttpServerPropertiesImpl::~HttpServerPropertiesImpl() {
}

void HttpServerPropertiesImpl::InitializeSpdyServers(
    std::vector<std::string>* spdy_servers,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (!spdy_servers)
    return;

  // Add the entries from persisted data.
  SpdyServerHostPortMap spdy_servers_map(SpdyServerHostPortMap::NO_AUTO_EVICT);
  for (std::vector<std::string>::reverse_iterator it = spdy_servers->rbegin();
       it != spdy_servers->rend(); ++it) {
    spdy_servers_map.Put(*it, support_spdy);
  }

  // |spdy_servers_map| will have the memory cache.
  spdy_servers_map_.Swap(spdy_servers_map);

  // Add the entries from the memory cache.
  for (SpdyServerHostPortMap::reverse_iterator it = spdy_servers_map.rbegin();
       it != spdy_servers_map.rend(); ++it) {
    // Add the entry if it is not in the cache, otherwise move it to the front
    // of recency list.
    if (spdy_servers_map_.Get(it->first) == spdy_servers_map_.end())
      spdy_servers_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::InitializeAlternativeServiceServers(
    AlternativeServiceMap* alternative_service_map) {
  int32_t size_diff =
      alternative_service_map->size() - alternative_service_map_.size();
  if (size_diff > 0) {
    UMA_HISTOGRAM_COUNTS("Net.AlternativeServiceServers.MorePrefsEntries",
                         size_diff);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Net.AlternativeServiceServers.MoreOrEqualCacheEntries", -size_diff);
  }

  AlternativeServiceMap new_alternative_service_map(
      AlternativeServiceMap::NO_AUTO_EVICT);
  // Add the entries from persisted data.
  for (AlternativeServiceMap::reverse_iterator input_it =
           alternative_service_map->rbegin();
       input_it != alternative_service_map->rend(); ++input_it) {
    DCHECK(!input_it->second.empty());
    new_alternative_service_map.Put(input_it->first, input_it->second);
  }

  alternative_service_map_.Swap(new_alternative_service_map);

  // Add the entries from the memory cache.
  for (AlternativeServiceMap::reverse_iterator input_it =
           new_alternative_service_map.rbegin();
       input_it != new_alternative_service_map.rend(); ++input_it) {
    if (alternative_service_map_.Get(input_it->first) ==
        alternative_service_map_.end()) {
      alternative_service_map_.Put(input_it->first, input_it->second);
    }
  }

  // Attempt to find canonical servers.
  uint16_t canonical_ports[] = {80, 443};
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    for (size_t j = 0; j < arraysize(canonical_ports); ++j) {
      HostPortPair canonical_host(canonical_suffix, canonical_ports[j]);
      // If we already have a valid canonical server, we're done.
      if (ContainsKey(canonical_host_to_origin_map_, canonical_host) &&
          (alternative_service_map_.Peek(
               canonical_host_to_origin_map_[canonical_host]) !=
           alternative_service_map_.end())) {
        continue;
      }
      // Now attempt to find a server which matches this origin and set it as
      // canonical.
      for (AlternativeServiceMap::const_iterator it =
               alternative_service_map_.begin();
           it != alternative_service_map_.end(); ++it) {
        if (base::EndsWith(it->first.host(), canonical_suffixes_[i],
                           base::CompareCase::INSENSITIVE_ASCII)) {
          canonical_host_to_origin_map_[canonical_host] = it->first;
          break;
        }
      }
    }
  }
}

void HttpServerPropertiesImpl::InitializeSpdySettingsServers(
    SpdySettingsMap* spdy_settings_map) {
  // Add the entries from persisted data.
  SpdySettingsMap new_spdy_settings_map(SpdySettingsMap::NO_AUTO_EVICT);
  for (SpdySettingsMap::reverse_iterator it = spdy_settings_map->rbegin();
       it != spdy_settings_map->rend(); ++it) {
    new_spdy_settings_map.Put(it->first, it->second);
  }

  spdy_settings_map_.Swap(new_spdy_settings_map);

  // Add the entries from the memory cache.
  for (SpdySettingsMap::reverse_iterator it = new_spdy_settings_map.rbegin();
       it != new_spdy_settings_map.rend(); ++it) {
    if (spdy_settings_map_.Get(it->first) == spdy_settings_map_.end())
      spdy_settings_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::InitializeSupportsQuic(IPAddress* last_address) {
  if (last_address)
    last_quic_address_ = *last_address;
}

void HttpServerPropertiesImpl::InitializeServerNetworkStats(
    ServerNetworkStatsMap* server_network_stats_map) {
  // Add the entries from persisted data.
  ServerNetworkStatsMap new_server_network_stats_map(
      ServerNetworkStatsMap::NO_AUTO_EVICT);
  for (ServerNetworkStatsMap::reverse_iterator it =
           server_network_stats_map->rbegin();
       it != server_network_stats_map->rend(); ++it) {
    new_server_network_stats_map.Put(it->first, it->second);
  }

  server_network_stats_map_.Swap(new_server_network_stats_map);

  // Add the entries from the memory cache.
  for (ServerNetworkStatsMap::reverse_iterator it =
           new_server_network_stats_map.rbegin();
       it != new_server_network_stats_map.rend(); ++it) {
    if (server_network_stats_map_.Get(it->first) ==
        server_network_stats_map_.end()) {
      server_network_stats_map_.Put(it->first, it->second);
    }
  }
}

void HttpServerPropertiesImpl::InitializeQuicServerInfoMap(
    QuicServerInfoMap* quic_server_info_map) {
  // Add the entries from persisted data.
  QuicServerInfoMap temp_map(QuicServerInfoMap::NO_AUTO_EVICT);
  for (QuicServerInfoMap::reverse_iterator it = quic_server_info_map->rbegin();
       it != quic_server_info_map->rend(); ++it) {
    temp_map.Put(it->first, it->second);
  }

  quic_server_info_map_.Swap(temp_map);

  // Add the entries from the memory cache.
  for (QuicServerInfoMap::reverse_iterator it = temp_map.rbegin();
       it != temp_map.rend(); ++it) {
    if (quic_server_info_map_.Get(it->first) == quic_server_info_map_.end()) {
      quic_server_info_map_.Put(it->first, it->second);
    }
  }
}

void HttpServerPropertiesImpl::GetSpdyServerList(
    base::ListValue* spdy_server_list,
    size_t max_size) const {
  DCHECK(CalledOnValidThread());
  DCHECK(spdy_server_list);
  spdy_server_list->Clear();
  size_t count = 0;
  // Get the list of servers (host/port) that support SPDY.
  for (SpdyServerHostPortMap::const_iterator it = spdy_servers_map_.begin();
       it != spdy_servers_map_.end() && count < max_size; ++it) {
    const std::string spdy_server_host_port = it->first;
    if (it->second) {
      spdy_server_list->Append(new base::StringValue(spdy_server_host_port));
      ++count;
    }
  }
}

base::WeakPtr<HttpServerProperties> HttpServerPropertiesImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HttpServerPropertiesImpl::Clear() {
  DCHECK(CalledOnValidThread());
  spdy_servers_map_.Clear();
  alternative_service_map_.Clear();
  canonical_host_to_origin_map_.clear();
  spdy_settings_map_.Clear();
  last_quic_address_ = IPAddress();
  server_network_stats_map_.Clear();
  quic_server_info_map_.Clear();
}

bool HttpServerPropertiesImpl::SupportsRequestPriority(
    const HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  if (GetSupportsSpdy(host_port_pair))
    return true;

  const AlternativeServiceVector alternative_service_vector =
      GetAlternativeServices(host_port_pair);
  for (const AlternativeService& alternative_service :
       alternative_service_vector) {
    if (alternative_service.protocol == QUIC) {
      return true;
    }
  }
  return false;
}

bool HttpServerPropertiesImpl::GetSupportsSpdy(
    const HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(host_port_pair.ToString());
  return spdy_host_port != spdy_servers_map_.end() && spdy_host_port->second;
}

void HttpServerPropertiesImpl::SetSupportsSpdy(
    const HostPortPair& host_port_pair,
    bool support_spdy) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return;

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(host_port_pair.ToString());
  if ((spdy_host_port != spdy_servers_map_.end()) &&
      (spdy_host_port->second == support_spdy)) {
    return;
  }
  // Cache the data.
  spdy_servers_map_.Put(host_port_pair.ToString(), support_spdy);
}

bool HttpServerPropertiesImpl::RequiresHTTP11(
    const HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  return (http11_servers_.find(host_port_pair) != http11_servers_.end());
}

void HttpServerPropertiesImpl::SetHTTP11Required(
    const HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return;

  http11_servers_.insert(host_port_pair);
}

void HttpServerPropertiesImpl::MaybeForceHTTP11(const HostPortPair& server,
                                                SSLConfig* ssl_config) {
  if (RequiresHTTP11(server)) {
    ForceHTTP11(ssl_config);
  }
}

std::string HttpServerPropertiesImpl::GetCanonicalSuffix(
    const std::string& host) {
  // If this host ends with a canonical suffix, then return the canonical
  // suffix.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (base::EndsWith(host, canonical_suffixes_[i],
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return canonical_suffix;
    }
  }
  return std::string();
}

AlternativeServiceVector HttpServerPropertiesImpl::GetAlternativeServices(
    const HostPortPair& origin) {
  // Copy alternative services with probability greater than or equal to the
  // threshold into |alternative_services_above_threshold|.
  AlternativeServiceVector alternative_services_above_threshold;
  const base::Time now = base::Time::Now();
  AlternativeServiceMap::iterator map_it = alternative_service_map_.Get(origin);
  if (map_it != alternative_service_map_.end()) {
    for (AlternativeServiceInfoVector::iterator it = map_it->second.begin();
         it != map_it->second.end();) {
      if (it->expiration < now) {
        it = map_it->second.erase(it);
        continue;
      }
      if (it->probability == 0 ||
          it->probability < alternative_service_probability_threshold_) {
        ++it;
        continue;
      }
      AlternativeService alternative_service(it->alternative_service);
      if (alternative_service.host.empty()) {
        alternative_service.host = origin.host();
      }
      // If the alternative service is equivalent to the origin (same host, same
      // port, and both TCP), then there is already a Job for it, so do not
      // return it here.
      if (origin.Equals(alternative_service.host_port_pair()) &&
          NPN_SPDY_MINIMUM_VERSION <= alternative_service.protocol &&
          alternative_service.protocol <= NPN_SPDY_MAXIMUM_VERSION) {
        ++it;
        continue;
      }
      alternative_services_above_threshold.push_back(alternative_service);
      ++it;
    }
    if (map_it->second.empty()) {
      alternative_service_map_.Erase(map_it);
    }
    return alternative_services_above_threshold;
  }

  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(origin);
  if (canonical == canonical_host_to_origin_map_.end()) {
    return AlternativeServiceVector();
  }
  map_it = alternative_service_map_.Get(canonical->second);
  if (map_it == alternative_service_map_.end()) {
    return AlternativeServiceVector();
  }
  for (AlternativeServiceInfoVector::iterator it = map_it->second.begin();
       it != map_it->second.end();) {
    if (it->expiration < now) {
      it = map_it->second.erase(it);
      continue;
    }
    if (it->probability < alternative_service_probability_threshold_) {
      ++it;
      continue;
    }
    AlternativeService alternative_service(it->alternative_service);
    if (alternative_service.host.empty()) {
      alternative_service.host = canonical->second.host();
      if (IsAlternativeServiceBroken(alternative_service)) {
        ++it;
        continue;
      }
      alternative_service.host = origin.host();
    } else if (IsAlternativeServiceBroken(alternative_service)) {
      ++it;
      continue;
    }
    alternative_services_above_threshold.push_back(alternative_service);
    ++it;
  }
  if (map_it->second.empty()) {
    alternative_service_map_.Erase(map_it);
  }
  return alternative_services_above_threshold;
}

bool HttpServerPropertiesImpl::SetAlternativeService(
    const HostPortPair& origin,
    const AlternativeService& alternative_service,
    double alternative_probability,
    base::Time expiration) {
  return SetAlternativeServices(
      origin, AlternativeServiceInfoVector(
                  /*size=*/1,
                  AlternativeServiceInfo(alternative_service,
                                         alternative_probability, expiration)));
}

bool HttpServerPropertiesImpl::SetAlternativeServices(
    const HostPortPair& origin,
    const AlternativeServiceInfoVector& alternative_service_info_vector) {
  AlternativeServiceMap::iterator it = alternative_service_map_.Peek(origin);

  if (alternative_service_info_vector.empty()) {
    if (it == alternative_service_map_.end()) {
      return false;
    }
    ClearAlternativeServices(origin);
    return true;
  }

  bool changed = true;
  if (it != alternative_service_map_.end()) {
    DCHECK(!it->second.empty());
    if (it->second.size() == alternative_service_info_vector.size()) {
      changed = !std::equal(it->second.begin(), it->second.end(),
                            alternative_service_info_vector.begin());
    }
  }

  const bool previously_no_alternative_services =
      (GetAlternateProtocolIterator(origin) == alternative_service_map_.end());

  alternative_service_map_.Put(origin, alternative_service_info_vector);

  if (previously_no_alternative_services &&
      !GetAlternativeServices(origin).empty()) {
    // TODO(rch): Consider the case where multiple requests are started
    // before the first completes. In this case, only one of the jobs
    // would reach this code, whereas all of them should should have.
    HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING);
  }

  // If this host ends with a canonical suffix, then set it as the
  // canonical host.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (base::EndsWith(origin.host(), canonical_suffixes_[i],
                       base::CompareCase::INSENSITIVE_ASCII)) {
      HostPortPair canonical_host(canonical_suffix, origin.port());
      canonical_host_to_origin_map_[canonical_host] = origin;
      break;
    }
  }

  return changed;
}

void HttpServerPropertiesImpl::MarkAlternativeServiceBroken(
    const AlternativeService& alternative_service) {
  // Empty host means use host of origin, callers are supposed to substitute.
  DCHECK(!alternative_service.host.empty());
  if (alternative_service.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL) {
    LOG(DFATAL) << "Trying to mark unknown alternate protocol broken.";
    return;
  }
  int count = ++recently_broken_alternative_services_[alternative_service];
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kBrokenAlternativeProtocolDelaySecs);
  base::TimeTicks when = base::TimeTicks::Now() + delay * (1 << (count - 1));
  auto result = broken_alternative_services_.insert(
      std::make_pair(alternative_service, when));
  // Return if alternative service is already in expiration queue.
  if (!result.second) {
    return;
  }

  // If this is the only entry in the list, schedule an expiration task.
  // Otherwise it will be rescheduled automatically when the pending task runs.
  if (broken_alternative_services_.size() == 1) {
    ScheduleBrokenAlternateProtocolMappingsExpiration();
  }
}

void HttpServerPropertiesImpl::MarkAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service) {
  if (!ContainsKey(recently_broken_alternative_services_, alternative_service))
    recently_broken_alternative_services_[alternative_service] = 1;
}

bool HttpServerPropertiesImpl::IsAlternativeServiceBroken(
    const AlternativeService& alternative_service) const {
  // Empty host means use host of origin, callers are supposed to substitute.
  DCHECK(!alternative_service.host.empty());
  return ContainsKey(broken_alternative_services_, alternative_service);
}

bool HttpServerPropertiesImpl::WasAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service) {
  if (alternative_service.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return false;
  return ContainsKey(recently_broken_alternative_services_,
                     alternative_service);
}

void HttpServerPropertiesImpl::ConfirmAlternativeService(
    const AlternativeService& alternative_service) {
  if (alternative_service.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return;
  broken_alternative_services_.erase(alternative_service);
  recently_broken_alternative_services_.erase(alternative_service);
}

void HttpServerPropertiesImpl::ClearAlternativeServices(
    const HostPortPair& origin) {
  RemoveCanonicalHost(origin);

  AlternativeServiceMap::iterator it = alternative_service_map_.Peek(origin);
  if (it == alternative_service_map_.end()) {
    return;
  }
  alternative_service_map_.Erase(it);
}

const AlternativeServiceMap& HttpServerPropertiesImpl::alternative_service_map()
    const {
  return alternative_service_map_;
}

scoped_ptr<base::Value>
HttpServerPropertiesImpl::GetAlternativeServiceInfoAsValue()
    const {
  scoped_ptr<base::ListValue> dict_list(new base::ListValue);
  for (const auto& alternative_service_map_item : alternative_service_map_) {
    scoped_ptr<base::ListValue> alternative_service_list(new base::ListValue);
    const HostPortPair& host_port_pair = alternative_service_map_item.first;
    for (const AlternativeServiceInfo& alternative_service_info :
         alternative_service_map_item.second) {
      std::string alternative_service_string(
          alternative_service_info.ToString());
      AlternativeService alternative_service(
          alternative_service_info.alternative_service);
      if (alternative_service.host.empty()) {
        alternative_service.host = host_port_pair.host();
      }
      if (IsAlternativeServiceBroken(alternative_service)) {
        alternative_service_string.append(" (broken)");
      }
      alternative_service_list->Append(
          new base::StringValue(alternative_service_string));
    }
    if (alternative_service_list->empty())
      continue;
    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("host_port_pair", host_port_pair.ToString());
    dict->Set("alternative_service",
              scoped_ptr<base::Value>(std::move(alternative_service_list)));
    dict_list->Append(std::move(dict));
  }
  return std::move(dict_list);
}

const SettingsMap& HttpServerPropertiesImpl::GetSpdySettings(
    const HostPortPair& host_port_pair) {
  SpdySettingsMap::iterator it = spdy_settings_map_.Get(host_port_pair);
  if (it == spdy_settings_map_.end()) {
    CR_DEFINE_STATIC_LOCAL(SettingsMap, kEmptySettingsMap, ());
    return kEmptySettingsMap;
  }
  return it->second;
}

bool HttpServerPropertiesImpl::SetSpdySetting(
    const HostPortPair& host_port_pair,
    SpdySettingsIds id,
    SpdySettingsFlags flags,
    uint32_t value) {
  if (!(flags & SETTINGS_FLAG_PLEASE_PERSIST))
      return false;

  SettingsFlagsAndValue flags_and_value(SETTINGS_FLAG_PERSISTED, value);
  SpdySettingsMap::iterator it = spdy_settings_map_.Get(host_port_pair);
  if (it == spdy_settings_map_.end()) {
    SettingsMap settings_map;
    settings_map[id] = flags_and_value;
    spdy_settings_map_.Put(host_port_pair, settings_map);
  } else {
    SettingsMap& settings_map = it->second;
    settings_map[id] = flags_and_value;
  }
  return true;
}

void HttpServerPropertiesImpl::ClearSpdySettings(
    const HostPortPair& host_port_pair) {
  SpdySettingsMap::iterator it = spdy_settings_map_.Peek(host_port_pair);
  if (it != spdy_settings_map_.end())
    spdy_settings_map_.Erase(it);
}

void HttpServerPropertiesImpl::ClearAllSpdySettings() {
  spdy_settings_map_.Clear();
}

const SpdySettingsMap&
HttpServerPropertiesImpl::spdy_settings_map() const {
  return spdy_settings_map_;
}

bool HttpServerPropertiesImpl::GetSupportsQuic(IPAddress* last_address) const {
  if (last_quic_address_.empty())
    return false;

  *last_address = last_quic_address_;
  return true;
}

void HttpServerPropertiesImpl::SetSupportsQuic(bool used_quic,
                                               const IPAddress& address) {
  if (!used_quic) {
    last_quic_address_ = IPAddress();
  } else {
    last_quic_address_ = address;
  }
}

void HttpServerPropertiesImpl::SetServerNetworkStats(
    const HostPortPair& host_port_pair,
    ServerNetworkStats stats) {
  server_network_stats_map_.Put(host_port_pair, stats);
}

const ServerNetworkStats* HttpServerPropertiesImpl::GetServerNetworkStats(
    const HostPortPair& host_port_pair) {
  ServerNetworkStatsMap::iterator it =
      server_network_stats_map_.Get(host_port_pair);
  if (it == server_network_stats_map_.end()) {
    return NULL;
  }
  return &it->second;
}

const ServerNetworkStatsMap&
HttpServerPropertiesImpl::server_network_stats_map() const {
  return server_network_stats_map_;
}

bool HttpServerPropertiesImpl::SetQuicServerInfo(
    const QuicServerId& server_id,
    const std::string& server_info) {
  QuicServerInfoMap::iterator it = quic_server_info_map_.Peek(server_id);
  bool changed =
      (it == quic_server_info_map_.end() || it->second != server_info);
  quic_server_info_map_.Put(server_id, server_info);
  return changed;
}

const std::string* HttpServerPropertiesImpl::GetQuicServerInfo(
    const QuicServerId& server_id) {
  QuicServerInfoMap::iterator it = quic_server_info_map_.Get(server_id);
  if (it == quic_server_info_map_.end())
    return nullptr;
  return &it->second;
}

const QuicServerInfoMap& HttpServerPropertiesImpl::quic_server_info_map()
    const {
  return quic_server_info_map_;
}

size_t HttpServerPropertiesImpl::max_server_configs_stored_in_properties()
    const {
  return max_server_configs_stored_in_properties_;
}

void HttpServerPropertiesImpl::SetMaxServerConfigsStoredInProperties(
    size_t max_server_configs_stored_in_properties) {
  max_server_configs_stored_in_properties_ =
      max_server_configs_stored_in_properties;

  // MRUCache doesn't allow the size of the cache to be changed. Thus create a
  // new map with the new size and add current elements and swap the new map.
  quic_server_info_map_.ShrinkToSize(max_server_configs_stored_in_properties_);
  QuicServerInfoMap temp_map(max_server_configs_stored_in_properties_);
  for (QuicServerInfoMap::reverse_iterator it = quic_server_info_map_.rbegin();
       it != quic_server_info_map_.rend(); ++it) {
    temp_map.Put(it->first, it->second);
  }

  quic_server_info_map_.Swap(temp_map);
}

void HttpServerPropertiesImpl::SetAlternativeServiceProbabilityThreshold(
    double threshold) {
  alternative_service_probability_threshold_ = threshold;
}

AlternativeServiceMap::const_iterator
HttpServerPropertiesImpl::GetAlternateProtocolIterator(
    const HostPortPair& server) {
  AlternativeServiceMap::const_iterator it =
      alternative_service_map_.Get(server);
  if (it != alternative_service_map_.end())
    return it;

  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(server);
  if (canonical == canonical_host_to_origin_map_.end()) {
    return alternative_service_map_.end();
  }

  const HostPortPair canonical_host_port = canonical->second;
  it = alternative_service_map_.Get(canonical_host_port);
  if (it == alternative_service_map_.end()) {
    return alternative_service_map_.end();
  }

  for (const AlternativeServiceInfo& alternative_service_info : it->second) {
    AlternativeService alternative_service(
        alternative_service_info.alternative_service);
    if (alternative_service.host.empty()) {
      alternative_service.host = canonical_host_port.host();
    }
    if (!IsAlternativeServiceBroken(alternative_service)) {
      return it;
    }
  }

  RemoveCanonicalHost(canonical_host_port);
  return alternative_service_map_.end();
}

HttpServerPropertiesImpl::CanonicalHostMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalHost(HostPortPair server) const {
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (base::EndsWith(server.host(), canonical_suffixes_[i],
                       base::CompareCase::INSENSITIVE_ASCII)) {
      HostPortPair canonical_host(canonical_suffix, server.port());
      return canonical_host_to_origin_map_.find(canonical_host);
    }
  }

  return canonical_host_to_origin_map_.end();
}

void HttpServerPropertiesImpl::RemoveCanonicalHost(
    const HostPortPair& server) {
  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(server);
  if (canonical == canonical_host_to_origin_map_.end())
    return;

  if (!canonical->second.Equals(server))
    return;

  canonical_host_to_origin_map_.erase(canonical->first);
}

void HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings() {
  base::TimeTicks now = base::TimeTicks::Now();
  while (!broken_alternative_services_.empty()) {
    BrokenAlternativeServices::iterator it =
        broken_alternative_services_.begin();
    if (now < it->second) {
      break;
    }

    const AlternativeService expired_alternative_service = it->first;
    broken_alternative_services_.erase(it);

    // Remove every occurrence of |expired_alternative_service| from
    // |alternative_service_map_|.
    for (AlternativeServiceMap::iterator map_it =
             alternative_service_map_.begin();
         map_it != alternative_service_map_.end();) {
      for (AlternativeServiceInfoVector::iterator it = map_it->second.begin();
           it != map_it->second.end();) {
        AlternativeService alternative_service(it->alternative_service);
        // Empty hostname in map means hostname of key: substitute before
        // comparing to |expired_alternative_service|.
        if (alternative_service.host.empty()) {
          alternative_service.host = map_it->first.host();
        }
        if (alternative_service == expired_alternative_service) {
          it = map_it->second.erase(it);
          continue;
        }
        ++it;
      }
      // If an origin has an empty list of alternative services, then remove it
      // from both |canonical_host_to_origin_map_| and
      // |alternative_service_map_|.
      if (map_it->second.empty()) {
        RemoveCanonicalHost(map_it->first);
        map_it = alternative_service_map_.Erase(map_it);
        continue;
      }
      ++map_it;
    }
  }
  ScheduleBrokenAlternateProtocolMappingsExpiration();
}

void
HttpServerPropertiesImpl::ScheduleBrokenAlternateProtocolMappingsExpiration() {
  if (broken_alternative_services_.empty()) {
    return;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks when = broken_alternative_services_.front().second;
  base::TimeDelta delay = when > now ? when - now : base::TimeDelta();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings,
          weak_ptr_factory_.GetWeakPtr()),
      delay);
}

}  // namespace net
