// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"

namespace net {

namespace {

const uint64 kBrokenAlternativeProtocolDelaySecs = 300;

}  // namespace

HttpServerPropertiesImpl::HttpServerPropertiesImpl()
    : spdy_servers_map_(SpdyServerHostPortMap::NO_AUTO_EVICT),
      alternative_service_map_(AlternativeServiceMap::NO_AUTO_EVICT),
      spdy_settings_map_(SpdySettingsMap::NO_AUTO_EVICT),
      server_network_stats_map_(ServerNetworkStatsMap::NO_AUTO_EVICT),
      alternative_service_probability_threshold_(1.0),
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
  for (std::vector<std::string>::reverse_iterator it = spdy_servers->rbegin();
       it != spdy_servers->rend(); ++it) {
    spdy_servers_map_.Put(*it, support_spdy);
  }
}

void HttpServerPropertiesImpl::InitializeAlternativeServiceServers(
    AlternativeServiceMap* alternative_service_map) {
  // Keep all the broken ones since those don't get persisted.
  for (AlternativeServiceMap::iterator it = alternative_service_map_.begin();
       it != alternative_service_map_.end();) {
    if (IsAlternativeServiceBroken(it->second.alternative_service)) {
      ++it;
    } else {
      it = alternative_service_map_.Erase(it);
    }
  }

  // Add the entries from persisted data.
  for (AlternativeServiceMap::reverse_iterator it =
           alternative_service_map->rbegin();
       it != alternative_service_map->rend(); ++it) {
    alternative_service_map_.Put(it->first, it->second);
  }

  // Attempt to find canonical servers.
  uint16 canonical_ports[] = { 80, 443 };
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
        if (base::EndsWith(it->first.host(), canonical_suffixes_[i], false)) {
          canonical_host_to_origin_map_[canonical_host] = it->first;
          break;
        }
      }
    }
  }
}

void HttpServerPropertiesImpl::InitializeSpdySettingsServers(
    SpdySettingsMap* spdy_settings_map) {
  for (SpdySettingsMap::reverse_iterator it = spdy_settings_map->rbegin();
       it != spdy_settings_map->rend(); ++it) {
    spdy_settings_map_.Put(it->first, it->second);
  }
}

void HttpServerPropertiesImpl::InitializeSupportsQuic(
    IPAddressNumber* last_address) {
  if (last_address)
    last_quic_address_ = *last_address;
}

void HttpServerPropertiesImpl::InitializeServerNetworkStats(
    ServerNetworkStatsMap* server_network_stats_map) {
  for (ServerNetworkStatsMap::reverse_iterator it =
           server_network_stats_map->rbegin();
       it != server_network_stats_map->rend(); ++it) {
    server_network_stats_map_.Put(it->first, it->second);
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
  last_quic_address_.clear();
  server_network_stats_map_.Clear();
}

bool HttpServerPropertiesImpl::SupportsRequestPriority(
    const HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  if (GetSupportsSpdy(host_port_pair))
    return true;

  const AlternativeService alternative_service =
      GetAlternativeService(host_port_pair);
  return alternative_service.protocol == QUIC;
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
    if (base::EndsWith(host, canonical_suffixes_[i], false)) {
      return canonical_suffix;
    }
  }
  return std::string();
}

AlternativeService HttpServerPropertiesImpl::GetAlternativeService(
    const HostPortPair& origin) {
  AlternativeServiceMap::const_iterator it =
      alternative_service_map_.Get(origin);
  if (it != alternative_service_map_.end()) {
    if (it->second.probability < alternative_service_probability_threshold_) {
      return AlternativeService();
    }
    AlternativeService alternative_service(it->second.alternative_service);
    if (alternative_service.host.empty()) {
      alternative_service.host = origin.host();
    }
    return alternative_service;
  }

  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(origin);
  if (canonical == canonical_host_to_origin_map_.end()) {
    return AlternativeService();
  }
  it = alternative_service_map_.Get(canonical->second);
  if (it == alternative_service_map_.end()) {
    return AlternativeService();
  }
  if (it->second.probability < alternative_service_probability_threshold_) {
    return AlternativeService();
  }
  AlternativeService alternative_service(it->second.alternative_service);
  if (alternative_service.host.empty()) {
    alternative_service.host = canonical->second.host();
  }
  if (IsAlternativeServiceBroken(alternative_service)) {
    RemoveCanonicalHost(canonical->second);
    return AlternativeService();
  }
  // Empty hostname: if alternative service for with hostname of canonical host
  // is not broken, then return alternative service with hostname of origin.
  if (it->second.alternative_service.host.empty()) {
    alternative_service.host = origin.host();
  }
  return alternative_service;
}

void HttpServerPropertiesImpl::SetAlternativeService(
    const HostPortPair& origin,
    const AlternativeService& alternative_service,
    double alternative_probability) {
  AlternativeService complete_alternative_service(alternative_service);
  if (complete_alternative_service.host.empty()) {
    complete_alternative_service.host = origin.host();
  }
  if (IsAlternativeServiceBroken(complete_alternative_service)) {
    DVLOG(1) << "Ignore alternative service since it is known to be broken.";
    return;
  }

  const AlternativeServiceInfo alternative_service_info(
      alternative_service, alternative_probability);
  AlternativeServiceMap::const_iterator it =
      GetAlternateProtocolIterator(origin);
  if (it != alternative_service_map_.end()) {
    const AlternativeServiceInfo existing_alternative_service_info = it->second;
    if (existing_alternative_service_info != alternative_service_info) {
      LOG(WARNING) << "Changing the alternative service for: "
                   << origin.ToString() << " from "
                   << existing_alternative_service_info.ToString() << " to "
                   << alternative_service_info.ToString() << ".";
    }
  } else {
    if (alternative_probability >= alternative_service_probability_threshold_) {
      // TODO(rch): Consider the case where multiple requests are started
      // before the first completes. In this case, only one of the jobs
      // would reach this code, whereas all of them should should have.
      HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING);
    }
  }

  alternative_service_map_.Put(origin, alternative_service_info);

  // If this host ends with a canonical suffix, then set it as the
  // canonical host.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (base::EndsWith(origin.host(), canonical_suffixes_[i], false)) {
      HostPortPair canonical_host(canonical_suffix, origin.port());
      canonical_host_to_origin_map_[canonical_host] = origin;
      break;
    }
  }
}

void HttpServerPropertiesImpl::MarkAlternativeServiceBroken(
    const AlternativeService& alternative_service) {
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

void HttpServerPropertiesImpl::ClearAlternativeService(
    const HostPortPair& origin) {
  RemoveCanonicalHost(origin);

  AlternativeServiceMap::iterator it = alternative_service_map_.Peek(origin);
  if (it == alternative_service_map_.end()) {
    return;
  }
  AlternativeService alternative_service(it->second.alternative_service);
  if (alternative_service.host.empty()) {
    alternative_service.host = origin.host();
  }
  alternative_service_map_.Erase(it);

  // The following is temporary to keep the existing semantics, which is that if
  // there is a broken alternative service in the mapping, then this method
  // leaves it in a non-broken, but recently broken state.
  //
  // TODO(bnc):
  //  1. Verify and document the class invariant that no broken alternative
  //     service can be in the mapping.
  //  2. Remove the rest of this method as it will be moot.
  broken_alternative_services_.erase(alternative_service);
}

const AlternativeServiceMap& HttpServerPropertiesImpl::alternative_service_map()
    const {
  return alternative_service_map_;
}

scoped_ptr<base::Value>
HttpServerPropertiesImpl::GetAlternativeServiceInfoAsValue()
    const {
  scoped_ptr<base::ListValue> dict_list(new base::ListValue());
  for (const auto& alternative_service_map_item : alternative_service_map_) {
    const HostPortPair& host_port_pair = alternative_service_map_item.first;
    const AlternativeServiceInfo& alternative_service_info =
        alternative_service_map_item.second;
    std::string alternative_service_string(alternative_service_info.ToString());
    AlternativeService alternative_service(
        alternative_service_info.alternative_service);
    if (alternative_service.host.empty()) {
      alternative_service.host = host_port_pair.host();
    }
    if (IsAlternativeServiceBroken(alternative_service)) {
      alternative_service_string.append(" (broken)");
    }

    scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetString("host_port_pair", host_port_pair.ToString());
    dict->SetString("alternative_service", alternative_service_string);
    dict_list->Append(dict.Pass());
  }
  return dict_list.Pass();
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
    uint32 value) {
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

bool HttpServerPropertiesImpl::GetSupportsQuic(
    IPAddressNumber* last_address) const {
  if (last_quic_address_.empty())
    return false;

  *last_address = last_quic_address_;
  return true;
}

void HttpServerPropertiesImpl::SetSupportsQuic(bool used_quic,
                                               const IPAddressNumber& address) {
  if (!used_quic) {
    last_quic_address_.clear();
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

  const AlternativeService alternative_service(
      it->second.alternative_service.protocol, canonical_host_port.host(),
      it->second.alternative_service.port);
  if (!IsAlternativeServiceBroken(alternative_service)) {
    return it;
  }

  RemoveCanonicalHost(canonical_host_port);
  return alternative_service_map_.end();
}

HttpServerPropertiesImpl::CanonicalHostMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalHost(HostPortPair server) const {
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (base::EndsWith(server.host(), canonical_suffixes_[i], false)) {
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

    const AlternativeService alternative_service = it->first;
    broken_alternative_services_.erase(it);
    // TODO(bnc): Make sure broken alternative services are not in the mapping.
    ClearAlternativeService(
        HostPortPair(alternative_service.host, alternative_service.port));
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
