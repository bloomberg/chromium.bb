// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace net {

namespace {

const uint64 kBrokenAlternateProtocolDelaySecs = 300;

}  // namespace

HttpServerPropertiesImpl::HttpServerPropertiesImpl()
    : spdy_servers_map_(SpdyServerHostPortMap::NO_AUTO_EVICT),
      alternate_protocol_map_(AlternateProtocolMap::NO_AUTO_EVICT),
      spdy_settings_map_(SpdySettingsMap::NO_AUTO_EVICT),
      server_network_stats_map_(ServerNetworkStatsMap::NO_AUTO_EVICT),
      alternate_protocol_probability_threshold_(1),
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

void HttpServerPropertiesImpl::InitializeAlternateProtocolServers(
    AlternateProtocolMap* alternate_protocol_map) {
  // Keep all the broken ones since those don't get persisted.
  for (AlternateProtocolMap::iterator it = alternate_protocol_map_.begin();
       it != alternate_protocol_map_.end();) {
    const AlternativeService alternative_service(
        it->second.protocol, it->first.host(), it->second.port);
    if (IsAlternativeServiceBroken(alternative_service)) {
      ++it;
    } else {
      it = alternate_protocol_map_.Erase(it);
    }
  }

  // Add the entries from persisted data.
  for (AlternateProtocolMap::reverse_iterator it =
           alternate_protocol_map->rbegin();
       it != alternate_protocol_map->rend(); ++it) {
    alternate_protocol_map_.Put(it->first, it->second);
  }

  // Attempt to find canonical servers.
  uint16 canonical_ports[] = { 80, 443 };
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    for (size_t j = 0; j < arraysize(canonical_ports); ++j) {
      HostPortPair canonical_host(canonical_suffix, canonical_ports[j]);
      // If we already have a valid canonical server, we're done.
      if (ContainsKey(canonical_host_to_origin_map_, canonical_host) &&
          (alternate_protocol_map_.Peek(canonical_host_to_origin_map_[
               canonical_host]) != alternate_protocol_map_.end())) {
        continue;
      }
      // Now attempt to find a server which matches this origin and set it as
      // canonical .
      for (AlternateProtocolMap::const_iterator it =
               alternate_protocol_map_.begin();
           it != alternate_protocol_map_.end(); ++it) {
        if (EndsWith(it->first.host(), canonical_suffixes_[i], false)) {
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

static const AlternateProtocolInfo* g_forced_alternate_protocol = NULL;

// static
void HttpServerPropertiesImpl::ForceAlternateProtocol(
    const AlternateProtocolInfo& info) {
  // Note: we're going to leak this.
  if (g_forced_alternate_protocol)
    delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = new AlternateProtocolInfo(info);
}

// static
void HttpServerPropertiesImpl::DisableForcedAlternateProtocol() {
  delete g_forced_alternate_protocol;
  g_forced_alternate_protocol = NULL;
}

base::WeakPtr<HttpServerProperties> HttpServerPropertiesImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HttpServerPropertiesImpl::Clear() {
  DCHECK(CalledOnValidThread());
  spdy_servers_map_.Clear();
  alternate_protocol_map_.Clear();
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

  SpdyServerHostPortMap::iterator spdy_host_port =
      spdy_servers_map_.Get(host_port_pair.ToString());
  if (spdy_host_port != spdy_servers_map_.end() && spdy_host_port->second)
    return true;

  const AlternativeService alternative_service =
      GetAlternativeService(host_port_pair);
  return alternative_service.protocol == QUIC;
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
    const net::HostPortPair& host_port_pair) {
  DCHECK(CalledOnValidThread());
  if (host_port_pair.host().empty())
    return false;

  return (http11_servers_.find(host_port_pair) != http11_servers_.end());
}

void HttpServerPropertiesImpl::SetHTTP11Required(
    const net::HostPortPair& host_port_pair) {
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
    if (EndsWith(host, canonical_suffixes_[i], false)) {
      return canonical_suffix;
    }
  }
  return std::string();
}

AlternativeService HttpServerPropertiesImpl::GetAlternativeService(
    const HostPortPair& origin) {
  AlternateProtocolMap::const_iterator it =
      GetAlternateProtocolIterator(origin);
  if (it != alternate_protocol_map_.end() &&
      it->second.probability >= alternate_protocol_probability_threshold_)
    return AlternativeService(it->second.protocol, origin.host(),
                              it->second.port);

  UMA_HISTOGRAM_BOOLEAN("Net.ForceAlternativeService",
                        g_forced_alternate_protocol != nullptr);
  if (g_forced_alternate_protocol)
    return AlternativeService(g_forced_alternate_protocol->protocol,
                              origin.host(), g_forced_alternate_protocol->port);

  AlternativeService uninitialize_alternative_service;
  return uninitialize_alternative_service;
}

void HttpServerPropertiesImpl::SetAlternateProtocol(
    const HostPortPair& origin,
    uint16 alternate_port,
    AlternateProtocol alternate_protocol,
    double alternate_probability) {
  const AlternativeService alternative_service(alternate_protocol,
                                               origin.host(), alternate_port);
  if (IsAlternativeServiceBroken(alternative_service)) {
    DVLOG(1) << "Ignore alternative service since it is known to be broken.";
    return;
  }

  const AlternateProtocolInfo alternate(alternate_port, alternate_protocol,
                                        alternate_probability);
  AlternateProtocolMap::const_iterator it =
      GetAlternateProtocolIterator(origin);
  if (it != alternate_protocol_map_.end()) {
    const AlternateProtocolInfo existing_alternate = it->second;

    if (!existing_alternate.Equals(alternate)) {
      LOG(WARNING) << "Changing the alternate protocol for: "
                   << origin.ToString()
                   << " from [Port: " << existing_alternate.port
                   << ", Protocol: " << existing_alternate.protocol
                   << ", Probability: " << existing_alternate.probability
                   << "] to [Port: " << alternate_port
                   << ", Protocol: " << alternate_protocol
                   << ", Probability: " << alternate_probability << "].";
    }
  } else {
    if (alternate_probability >= alternate_protocol_probability_threshold_) {
      // TODO(rch): Consider the case where multiple requests are started
      // before the first completes. In this case, only one of the jobs
      // would reach this code, whereas all of them should should have.
      HistogramAlternateProtocolUsage(ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING);
    }
  }

  alternate_protocol_map_.Put(origin, alternate);

  // If this host ends with a canonical suffix, then set it as the
  // canonical host.
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(origin.host(), canonical_suffixes_[i], false)) {
      HostPortPair canonical_host(canonical_suffix, origin.port());
      canonical_host_to_origin_map_[canonical_host] = origin;
      break;
    }
  }
}

void HttpServerPropertiesImpl::SetBrokenAlternateProtocol(
    const HostPortPair& origin) {
  const AlternativeService alternative_service = GetAlternativeService(origin);
  if (alternative_service.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL) {
    LOG(DFATAL) << "Trying to mark unknown alternate protocol broken.";
    return;
  }
  int count = ++recently_broken_alternative_services_[alternative_service];
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(kBrokenAlternateProtocolDelaySecs);
  base::TimeTicks when = base::TimeTicks::Now() + delay * (1 << (count - 1));
  auto result = broken_alternative_services_.insert(
      std::make_pair(alternative_service, when));
  // Return if alternative service is already in expiration queue.
  if (!result.second) {
    return;
  }

  // Do not leave this host as canonical so that we don't infer the other
  // hosts are also broken without testing them first.
  RemoveCanonicalHost(origin);

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
    const AlternativeService& alternative_service) {
  return ContainsKey(broken_alternative_services_, alternative_service);
}

bool HttpServerPropertiesImpl::WasAlternateProtocolRecentlyBroken(
    const HostPortPair& origin) {
  const AlternativeService alternative_service = GetAlternativeService(origin);
  if (alternative_service.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return false;
  return ContainsKey(recently_broken_alternative_services_,
                     alternative_service);
}

void HttpServerPropertiesImpl::ConfirmAlternateProtocol(
    const HostPortPair& origin) {
  const AlternativeService alternative_service = GetAlternativeService(origin);
  if (alternative_service.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return;
  broken_alternative_services_.erase(alternative_service);
  recently_broken_alternative_services_.erase(alternative_service);
}

void HttpServerPropertiesImpl::ClearAlternateProtocol(
    const HostPortPair& origin) {
  RemoveCanonicalHost(origin);

  AlternateProtocolMap::iterator it = alternate_protocol_map_.Peek(origin);
  if (it == alternate_protocol_map_.end()) {
    return;
  }
  const AlternativeService alternative_service(
      it->second.protocol, it->first.host(), it->second.port);
  alternate_protocol_map_.Erase(it);

  // The following is temporary to keep the existing semantics, which is that if
  // there is a broken alternative service in the mapping, then this method
  // leaves it in a non-broken, but recently broken state.
  //
  // TODO(bnc):
  //  1. Verify and document the class invariant that no broken alternative
  //     service can be in the mapping.
  //  2. Remove the rest of this method as it will be moot.
  //  3. Provide a SetAlternativeServiceRecentlyBroken if necessary.
  broken_alternative_services_.erase(alternative_service);
}

const AlternateProtocolMap&
HttpServerPropertiesImpl::alternate_protocol_map() const {
  return alternate_protocol_map_;
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

void HttpServerPropertiesImpl::SetAlternateProtocolProbabilityThreshold(
    double threshold) {
  alternate_protocol_probability_threshold_ = threshold;
}

AlternateProtocolMap::const_iterator
HttpServerPropertiesImpl::GetAlternateProtocolIterator(
    const HostPortPair& server) {
  AlternateProtocolMap::const_iterator it = alternate_protocol_map_.Get(server);
  if (it != alternate_protocol_map_.end())
    return it;

  CanonicalHostMap::const_iterator canonical = GetCanonicalHost(server);
  if (canonical != canonical_host_to_origin_map_.end())
    return alternate_protocol_map_.Get(canonical->second);

  return alternate_protocol_map_.end();
}

HttpServerPropertiesImpl::CanonicalHostMap::const_iterator
HttpServerPropertiesImpl::GetCanonicalHost(HostPortPair server) const {
  for (size_t i = 0; i < canonical_suffixes_.size(); ++i) {
    std::string canonical_suffix = canonical_suffixes_[i];
    if (EndsWith(server.host(), canonical_suffixes_[i], false)) {
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
    ClearAlternateProtocol(
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
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesImpl::ExpireBrokenAlternateProtocolMappings,
          weak_ptr_factory_.GetWeakPtr()),
      delay);
}

}  // namespace net
