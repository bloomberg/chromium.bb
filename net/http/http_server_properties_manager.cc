// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_manager.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_util.h"

namespace net {

namespace {

// Time to wait before starting an update the http_server_properties_impl_ cache
// from preferences. Scheduling another update during this period will reset the
// timer.
const int64 kUpdateCacheDelayMs = 1000;

// Time to wait before starting an update the preferences from the
// http_server_properties_impl_ cache. Scheduling another update during this
// period will reset the timer.
const int64 kUpdatePrefsDelayMs = 5000;

// "version" 0 indicates, http_server_properties doesn't have "version"
// property.
const int kMissingVersion = 0;

// The version number of persisted http_server_properties.
const int kVersionNumber = 3;

typedef std::vector<std::string> StringVector;

// Persist 200 MRU AlternateProtocolHostPortPairs.
const int kMaxAlternateProtocolHostsToPersist = 200;

// Persist 200 MRU SpdySettingsHostPortPairs.
const int kMaxSpdySettingsHostsToPersist = 200;

// Persist 300 MRU SupportsSpdyServerHostPortPairs.
const int kMaxSupportsSpdyServerHostsToPersist = 300;

// Persist 200 ServerNetworkStats.
const int kMaxServerNetworkStatsHostsToPersist = 200;

const char kVersionKey[] = "version";
const char kServersKey[] = "servers";
const char kSupportsSpdyKey[] = "supports_spdy";
const char kSettingsKey[] = "settings";
const char kSupportsQuicKey[] = "supports_quic";
const char kUsedQuicKey[] = "used_quic";
const char kAddressKey[] = "address";
const char kAlternateProtocolKey[] = "alternate_protocol";
const char kPortKey[] = "port";
const char kProtocolKey[] = "protocol_str";
const char kProbabilityKey[] = "probability";
const char kNetworkStatsKey[] = "network_stats";
const char kSrttKey[] = "srtt";

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  HttpServerPropertiesManager

HttpServerPropertiesManager::HttpServerPropertiesManager(
    PrefService* pref_service,
    const char* pref_path,
    scoped_refptr<base::SequencedTaskRunner> network_task_runner)
    : pref_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      pref_service_(pref_service),
      setting_prefs_(false),
      path_(pref_path),
      network_task_runner_(network_task_runner) {
  DCHECK(pref_service);
  pref_weak_ptr_factory_.reset(
      new base::WeakPtrFactory<HttpServerPropertiesManager>(this));
  pref_weak_ptr_ = pref_weak_ptr_factory_->GetWeakPtr();
  pref_cache_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      path_,
      base::Bind(&HttpServerPropertiesManager::OnHttpServerPropertiesChanged,
                 base::Unretained(this)));
}

HttpServerPropertiesManager::~HttpServerPropertiesManager() {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  network_weak_ptr_factory_.reset();
}

void HttpServerPropertiesManager::InitializeOnNetworkThread() {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  network_weak_ptr_factory_.reset(
      new base::WeakPtrFactory<HttpServerPropertiesManager>(this));
  http_server_properties_impl_.reset(new HttpServerPropertiesImpl());

  network_prefs_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);

  pref_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdateCacheFromPrefsOnPrefThread,
                 pref_weak_ptr_));
}

void HttpServerPropertiesManager::ShutdownOnPrefThread() {
  DCHECK(pref_task_runner_->RunsTasksOnCurrentThread());
  // Cancel any pending updates, and stop listening for pref change updates.
  pref_cache_update_timer_->Stop();
  pref_weak_ptr_factory_.reset();
  pref_change_registrar_.RemoveAll();
}

// static
void HttpServerPropertiesManager::SetVersion(
    base::DictionaryValue* http_server_properties_dict,
    int version_number) {
  if (version_number < 0)
    version_number = kVersionNumber;
  DCHECK_LE(version_number, kVersionNumber);
  if (version_number <= kVersionNumber)
    http_server_properties_dict->SetInteger(kVersionKey, version_number);
}

// This is required for conformance with the HttpServerProperties interface.
base::WeakPtr<HttpServerProperties> HttpServerPropertiesManager::GetWeakPtr() {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return network_weak_ptr_factory_->GetWeakPtr();
}

void HttpServerPropertiesManager::Clear() {
  Clear(base::Closure());
}

void HttpServerPropertiesManager::Clear(const base::Closure& completion) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  http_server_properties_impl_->Clear();
  UpdatePrefsFromCacheOnNetworkThread(completion);
}

bool HttpServerPropertiesManager::SupportsRequestPriority(
    const HostPortPair& server) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->SupportsRequestPriority(server);
}

void HttpServerPropertiesManager::SetSupportsSpdy(const HostPortPair& server,
                                                  bool support_spdy) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  http_server_properties_impl_->SetSupportsSpdy(server, support_spdy);
  ScheduleUpdatePrefsOnNetworkThread();
}

bool HttpServerPropertiesManager::RequiresHTTP11(const HostPortPair& server) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->RequiresHTTP11(server);
}

void HttpServerPropertiesManager::SetHTTP11Required(
    const HostPortPair& server) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  http_server_properties_impl_->SetHTTP11Required(server);
  ScheduleUpdatePrefsOnNetworkThread();
}

void HttpServerPropertiesManager::MaybeForceHTTP11(const HostPortPair& server,
                                                   SSLConfig* ssl_config) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->MaybeForceHTTP11(server, ssl_config);
}

AlternativeService HttpServerPropertiesManager::GetAlternativeService(
    const HostPortPair& origin) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->GetAlternativeService(origin);
}

void HttpServerPropertiesManager::SetAlternateProtocol(
    const HostPortPair& origin,
    uint16 alternate_port,
    AlternateProtocol alternate_protocol,
    double alternate_probability) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->SetAlternateProtocol(
      origin, alternate_port, alternate_protocol, alternate_probability);
  ScheduleUpdatePrefsOnNetworkThread();
}

void HttpServerPropertiesManager::SetBrokenAlternateProtocol(
    const HostPortPair& origin) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->SetBrokenAlternateProtocol(origin);
  ScheduleUpdatePrefsOnNetworkThread();
}

void HttpServerPropertiesManager::MarkAlternativeServiceRecentlyBroken(
    const AlternativeService& alternative_service) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->MarkAlternativeServiceRecentlyBroken(
      alternative_service);
  ScheduleUpdatePrefsOnNetworkThread();
}

bool HttpServerPropertiesManager::IsAlternativeServiceBroken(
    const AlternativeService& alternative_service) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->IsAlternativeServiceBroken(
      alternative_service);
}

bool HttpServerPropertiesManager::WasAlternateProtocolRecentlyBroken(
    const HostPortPair& origin) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->WasAlternateProtocolRecentlyBroken(
      origin);
}

void HttpServerPropertiesManager::ConfirmAlternateProtocol(
    const HostPortPair& origin) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->ConfirmAlternateProtocol(origin);
  ScheduleUpdatePrefsOnNetworkThread();
}

void HttpServerPropertiesManager::ClearAlternateProtocol(
    const HostPortPair& origin) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->ClearAlternateProtocol(origin);
  ScheduleUpdatePrefsOnNetworkThread();
}

const AlternateProtocolMap&
HttpServerPropertiesManager::alternate_protocol_map() const {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->alternate_protocol_map();
}

void HttpServerPropertiesManager::SetAlternateProtocolProbabilityThreshold(
    double threshold) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->SetAlternateProtocolProbabilityThreshold(
      threshold);
}

const SettingsMap& HttpServerPropertiesManager::GetSpdySettings(
    const HostPortPair& host_port_pair) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->GetSpdySettings(host_port_pair);
}

bool HttpServerPropertiesManager::SetSpdySetting(
    const HostPortPair& host_port_pair,
    SpdySettingsIds id,
    SpdySettingsFlags flags,
    uint32 value) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  bool persist = http_server_properties_impl_->SetSpdySetting(
      host_port_pair, id, flags, value);
  if (persist)
    ScheduleUpdatePrefsOnNetworkThread();
  return persist;
}

void HttpServerPropertiesManager::ClearSpdySettings(
    const HostPortPair& host_port_pair) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->ClearSpdySettings(host_port_pair);
  ScheduleUpdatePrefsOnNetworkThread();
}

void HttpServerPropertiesManager::ClearAllSpdySettings() {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->ClearAllSpdySettings();
  ScheduleUpdatePrefsOnNetworkThread();
}

const SpdySettingsMap& HttpServerPropertiesManager::spdy_settings_map()
    const {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->spdy_settings_map();
}

bool HttpServerPropertiesManager::GetSupportsQuic(
    IPAddressNumber* last_address) const {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->GetSupportsQuic(last_address);
}

void HttpServerPropertiesManager::SetSupportsQuic(
    bool used_quic,
    const IPAddressNumber& address) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->SetSupportsQuic(used_quic, address);
  ScheduleUpdatePrefsOnNetworkThread();
}

void HttpServerPropertiesManager::SetServerNetworkStats(
    const HostPortPair& host_port_pair,
    ServerNetworkStats stats) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  http_server_properties_impl_->SetServerNetworkStats(host_port_pair, stats);
  ScheduleUpdatePrefsOnNetworkThread();
}

const ServerNetworkStats* HttpServerPropertiesManager::GetServerNetworkStats(
    const HostPortPair& host_port_pair) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->GetServerNetworkStats(host_port_pair);
}

const ServerNetworkStatsMap&
HttpServerPropertiesManager::server_network_stats_map() const {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  return http_server_properties_impl_->server_network_stats_map();
}

//
// Update the HttpServerPropertiesImpl's cache with data from preferences.
//
void HttpServerPropertiesManager::ScheduleUpdateCacheOnPrefThread() {
  DCHECK(pref_task_runner_->RunsTasksOnCurrentThread());
  // Cancel pending updates, if any.
  pref_cache_update_timer_->Stop();
  StartCacheUpdateTimerOnPrefThread(
      base::TimeDelta::FromMilliseconds(kUpdateCacheDelayMs));
}

void HttpServerPropertiesManager::StartCacheUpdateTimerOnPrefThread(
    base::TimeDelta delay) {
  DCHECK(pref_task_runner_->RunsTasksOnCurrentThread());
  pref_cache_update_timer_->Start(
      FROM_HERE,
      delay,
      this,
      &HttpServerPropertiesManager::UpdateCacheFromPrefsOnPrefThread);
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnPrefThread() {
  // The preferences can only be read on the pref thread.
  DCHECK(pref_task_runner_->RunsTasksOnCurrentThread());

  if (!pref_service_->HasPrefPath(path_))
    return;

  bool detected_corrupted_prefs = false;
  const base::DictionaryValue& http_server_properties_dict =
      *pref_service_->GetDictionary(path_);

  int version = kMissingVersion;
  if (!http_server_properties_dict.GetIntegerWithoutPathExpansion(kVersionKey,
                                                                  &version)) {
    DVLOG(1) << "Missing version. Clearing all properties.";
    return;
  }

  // The properties for a given server is in
  // http_server_properties_dict["servers"][server].
  const base::DictionaryValue* servers_dict = NULL;
  if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
          kServersKey, &servers_dict)) {
    DVLOG(1) << "Malformed http_server_properties for servers.";
    return;
  }

  IPAddressNumber* addr = new IPAddressNumber;
  ReadSupportsQuic(http_server_properties_dict, addr);

  // String is host/port pair of spdy server.
  scoped_ptr<StringVector> spdy_servers(new StringVector);
  scoped_ptr<SpdySettingsMap> spdy_settings_map(
      new SpdySettingsMap(kMaxSpdySettingsHostsToPersist));
  scoped_ptr<AlternateProtocolMap> alternate_protocol_map(
      new AlternateProtocolMap(kMaxAlternateProtocolHostsToPersist));
  scoped_ptr<ServerNetworkStatsMap> server_network_stats_map(
      new ServerNetworkStatsMap(kMaxServerNetworkStatsHostsToPersist));

  for (base::DictionaryValue::Iterator it(*servers_dict); !it.IsAtEnd();
       it.Advance()) {
    // Get server's host/pair.
    const std::string& server_str = it.key();
    HostPortPair server = HostPortPair::FromString(server_str);
    if (server.host().empty()) {
      DVLOG(1) << "Malformed http_server_properties for server: " << server_str;
      detected_corrupted_prefs = true;
      continue;
    }

    const base::DictionaryValue* server_pref_dict = NULL;
    if (!it.value().GetAsDictionary(&server_pref_dict)) {
      DVLOG(1) << "Malformed http_server_properties server: " << server_str;
      detected_corrupted_prefs = true;
      continue;
    }

    // Get if server supports Spdy.
    bool supports_spdy = false;
    if ((server_pref_dict->GetBoolean(kSupportsSpdyKey, &supports_spdy)) &&
        supports_spdy) {
      spdy_servers->push_back(server_str);
    }

    AddToSpdySettingsMap(server, *server_pref_dict, spdy_settings_map.get());
    if (!AddToAlternateProtocolMap(server, *server_pref_dict,
                                   alternate_protocol_map.get()) ||
        !AddToNetworkStatsMap(server, *server_pref_dict,
                              server_network_stats_map.get())) {
      detected_corrupted_prefs = true;
    }
  }

  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesManager::UpdateCacheFromPrefsOnNetworkThread,
          base::Unretained(this), base::Owned(spdy_servers.release()),
          base::Owned(spdy_settings_map.release()),
          base::Owned(alternate_protocol_map.release()), base::Owned(addr),
          base::Owned(server_network_stats_map.release()),
          detected_corrupted_prefs));
}

void HttpServerPropertiesManager::AddToSpdySettingsMap(
    const HostPortPair& server,
    const base::DictionaryValue& server_pref_dict,
    SpdySettingsMap* spdy_settings_map) {
  // Get SpdySettings.
  DCHECK(spdy_settings_map->Peek(server) == spdy_settings_map->end());
  const base::DictionaryValue* spdy_settings_dict = NULL;
  if (!server_pref_dict.GetDictionaryWithoutPathExpansion(
          kSettingsKey, &spdy_settings_dict)) {
    return;
  }
  SettingsMap settings_map;
  for (base::DictionaryValue::Iterator dict_it(*spdy_settings_dict);
       !dict_it.IsAtEnd(); dict_it.Advance()) {
    const std::string& id_str = dict_it.key();
    int id = 0;
    if (!base::StringToInt(id_str, &id)) {
      DVLOG(1) << "Malformed id in SpdySettings for server: "
               << server.ToString();
      NOTREACHED();
      continue;
    }
    int value = 0;
    if (!dict_it.value().GetAsInteger(&value)) {
      DVLOG(1) << "Malformed value in SpdySettings for server: "
               << server.ToString();
      NOTREACHED();
      continue;
    }
    SettingsFlagsAndValue flags_and_value(SETTINGS_FLAG_PERSISTED, value);
    settings_map[static_cast<SpdySettingsIds>(id)] = flags_and_value;
  }
  spdy_settings_map->Put(server, settings_map);
}

AlternateProtocolInfo HttpServerPropertiesManager::ParseAlternateProtocolDict(
    const base::DictionaryValue& alternate_protocol_dict,
    const std::string& server_str) {
  AlternateProtocolInfo alternate_protocol;
  int port = 0;
  if (!alternate_protocol_dict.GetInteger(kPortKey, &port) ||
      !IsPortValid(port)) {
    DVLOG(1) << "Malformed alternative service port for server: " << server_str;
    return alternate_protocol;
  }
  alternate_protocol.port = static_cast<uint16>(port);

  double probability = 1.0;
  if (alternate_protocol_dict.HasKey(kProbabilityKey) &&
      !alternate_protocol_dict.GetDoubleWithoutPathExpansion(kProbabilityKey,
                                                             &probability)) {
    DVLOG(1) << "Malformed alternative service probability for server: "
             << server_str;
    return alternate_protocol;
  }
  alternate_protocol.probability = probability;

  std::string protocol_str;
  if (!alternate_protocol_dict.GetStringWithoutPathExpansion(kProtocolKey,
                                                             &protocol_str)) {
    DVLOG(1) << "Malformed alternative service protocol string for server: "
             << server_str;
    return alternate_protocol;
  }
  AlternateProtocol protocol = AlternateProtocolFromString(protocol_str);
  if (!IsAlternateProtocolValid(protocol)) {
    DVLOG(1) << "Invalid alternative service protocol string for server: "
             << server_str;
    return alternate_protocol;
  }
  alternate_protocol.protocol = protocol;

  return alternate_protocol;
}

bool HttpServerPropertiesManager::AddToAlternateProtocolMap(
    const HostPortPair& server,
    const base::DictionaryValue& server_pref_dict,
    AlternateProtocolMap* alternate_protocol_map) {
  // Get alternate_protocol server.
  DCHECK(alternate_protocol_map->Peek(server) == alternate_protocol_map->end());
  const base::DictionaryValue* alternate_protocol_dict = NULL;
  if (!server_pref_dict.GetDictionaryWithoutPathExpansion(
          kAlternateProtocolKey, &alternate_protocol_dict)) {
    return true;
  }
  AlternateProtocolInfo alternate_protocol =
      ParseAlternateProtocolDict(*alternate_protocol_dict, server.ToString());
  if (alternate_protocol.protocol == UNINITIALIZED_ALTERNATE_PROTOCOL)
    return false;
  alternate_protocol_map->Put(server, alternate_protocol);
  return true;
}

bool HttpServerPropertiesManager::ReadSupportsQuic(
    const base::DictionaryValue& http_server_properties_dict,
    IPAddressNumber* last_quic_address) {
  const base::DictionaryValue* supports_quic_dict = NULL;
  if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
          kSupportsQuicKey, &supports_quic_dict)) {
    return true;
  }
  bool used_quic = false;
  if (!supports_quic_dict->GetBooleanWithoutPathExpansion(kUsedQuicKey,
                                                          &used_quic)) {
    DVLOG(1) << "Malformed SupportsQuic";
    return false;
  }
  if (!used_quic)
    return false;

  std::string address;
  if (!supports_quic_dict->GetStringWithoutPathExpansion(kAddressKey,
                                                         &address) ||
      !ParseIPLiteralToNumber(address, last_quic_address)) {
    DVLOG(1) << "Malformed SupportsQuic";
    return false;
  }
  return true;
}

bool HttpServerPropertiesManager::AddToNetworkStatsMap(
    const HostPortPair& server,
    const base::DictionaryValue& server_pref_dict,
    ServerNetworkStatsMap* network_stats_map) {
  DCHECK(network_stats_map->Peek(server) == network_stats_map->end());
  const base::DictionaryValue* server_network_stats_dict = NULL;
  if (!server_pref_dict.GetDictionaryWithoutPathExpansion(
          kNetworkStatsKey, &server_network_stats_dict)) {
    return true;
  }
  int srtt;
  if (!server_network_stats_dict->GetIntegerWithoutPathExpansion(kSrttKey,
                                                                 &srtt)) {
    DVLOG(1) << "Malformed ServerNetworkStats for server: "
             << server.ToString();
    return false;
  }
  ServerNetworkStats server_network_stats;
  server_network_stats.srtt = base::TimeDelta::FromInternalValue(srtt);
  // TODO(rtenneti): When QUIC starts using bandwidth_estimate, then persist
  // bandwidth_estimate.
  network_stats_map->Put(server, server_network_stats);
  return true;
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnNetworkThread(
    StringVector* spdy_servers,
    SpdySettingsMap* spdy_settings_map,
    AlternateProtocolMap* alternate_protocol_map,
    IPAddressNumber* last_quic_address,
    ServerNetworkStatsMap* server_network_stats_map,
    bool detected_corrupted_prefs) {
  // Preferences have the master data because admins might have pushed new
  // preferences. Update the cached data with new data from preferences.
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  UMA_HISTOGRAM_COUNTS("Net.CountOfSpdyServers", spdy_servers->size());
  http_server_properties_impl_->InitializeSpdyServers(spdy_servers, true);

  // Update the cached data and use the new spdy_settings from preferences.
  UMA_HISTOGRAM_COUNTS("Net.CountOfSpdySettings", spdy_settings_map->size());
  http_server_properties_impl_->InitializeSpdySettingsServers(
      spdy_settings_map);

  // Update the cached data and use the new Alternate-Protocol server list from
  // preferences.
  UMA_HISTOGRAM_COUNTS("Net.CountOfAlternateProtocolServers",
                       alternate_protocol_map->size());
  http_server_properties_impl_->InitializeAlternateProtocolServers(
      alternate_protocol_map);

  http_server_properties_impl_->InitializeSupportsQuic(last_quic_address);

  http_server_properties_impl_->InitializeServerNetworkStats(
      server_network_stats_map);

  // Update the prefs with what we have read (delete all corrupted prefs).
  if (detected_corrupted_prefs)
    ScheduleUpdatePrefsOnNetworkThread();
}

//
// Update Preferences with data from the cached data.
//
void HttpServerPropertiesManager::ScheduleUpdatePrefsOnNetworkThread() {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  // Cancel pending updates, if any.
  network_prefs_update_timer_->Stop();
  StartPrefsUpdateTimerOnNetworkThread(
      base::TimeDelta::FromMilliseconds(kUpdatePrefsDelayMs));
}

void HttpServerPropertiesManager::StartPrefsUpdateTimerOnNetworkThread(
    base::TimeDelta delay) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());
  // This is overridden in tests to post the task without the delay.
  network_prefs_update_timer_->Start(
      FROM_HERE,
      delay,
      this,
      &HttpServerPropertiesManager::UpdatePrefsFromCacheOnNetworkThread);
}

// This is required so we can set this as the callback for a timer.
void HttpServerPropertiesManager::UpdatePrefsFromCacheOnNetworkThread() {
  UpdatePrefsFromCacheOnNetworkThread(base::Closure());
}

void HttpServerPropertiesManager::UpdatePrefsFromCacheOnNetworkThread(
    const base::Closure& completion) {
  DCHECK(network_task_runner_->RunsTasksOnCurrentThread());

  base::ListValue* spdy_server_list = new base::ListValue;
  http_server_properties_impl_->GetSpdyServerList(
      spdy_server_list, kMaxSupportsSpdyServerHostsToPersist);

  SpdySettingsMap* spdy_settings_map =
      new SpdySettingsMap(kMaxSpdySettingsHostsToPersist);
  const SpdySettingsMap& main_map =
      http_server_properties_impl_->spdy_settings_map();
  int count = 0;
  for (SpdySettingsMap::const_iterator it = main_map.begin();
       it != main_map.end() && count < kMaxSpdySettingsHostsToPersist;
       ++it, ++count) {
    spdy_settings_map->Put(it->first, it->second);
  }

  AlternateProtocolMap* alternate_protocol_map =
      new AlternateProtocolMap(kMaxAlternateProtocolHostsToPersist);
  const AlternateProtocolMap& map =
      http_server_properties_impl_->alternate_protocol_map();
  count = 0;
  typedef std::map<std::string, bool> CanonicalHostPersistedMap;
  CanonicalHostPersistedMap persisted_map;
  for (AlternateProtocolMap::const_iterator it = map.begin();
       it != map.end() && count < kMaxAlternateProtocolHostsToPersist; ++it) {
    const AlternateProtocolInfo& alternate_protocol = it->second;
    if (!IsAlternateProtocolValid(alternate_protocol.protocol)) {
      continue;
    }
    const HostPortPair& server = it->first;
    if (IsAlternativeServiceBroken(
            AlternativeService(alternate_protocol.protocol, server.host(),
                               alternate_protocol.port))) {
      continue;
    }
    std::string canonical_suffix =
        http_server_properties_impl_->GetCanonicalSuffix(server.host());
    if (!canonical_suffix.empty()) {
      if (persisted_map.find(canonical_suffix) != persisted_map.end())
        continue;
      persisted_map[canonical_suffix] = true;
    }
    alternate_protocol_map->Put(server, alternate_protocol);
    ++count;
  }

  ServerNetworkStatsMap* server_network_stats_map =
      new ServerNetworkStatsMap(kMaxServerNetworkStatsHostsToPersist);
  const ServerNetworkStatsMap& main_server_network_stats_map =
      http_server_properties_impl_->server_network_stats_map();
  for (ServerNetworkStatsMap::const_iterator it =
           main_server_network_stats_map.begin();
       it != main_server_network_stats_map.end(); ++it) {
    server_network_stats_map->Put(it->first, it->second);
  }

  IPAddressNumber* last_quic_addr = new IPAddressNumber;
  http_server_properties_impl_->GetSupportsQuic(last_quic_addr);
  // Update the preferences on the pref thread.
  pref_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesManager::UpdatePrefsOnPrefThread, pref_weak_ptr_,
          base::Owned(spdy_server_list), base::Owned(spdy_settings_map),
          base::Owned(alternate_protocol_map), base::Owned(last_quic_addr),
          base::Owned(server_network_stats_map), completion));
}

// A local or temporary data structure to hold |supports_spdy|, SpdySettings,
// AlternateProtocolInfo and SupportsQuic preferences for a server. This is used
// only in UpdatePrefsOnPrefThread.
struct ServerPref {
  ServerPref()
      : supports_spdy(false),
        settings_map(NULL),
        alternate_protocol(NULL),
        supports_quic(NULL),
        server_network_stats(NULL) {}
  ServerPref(bool supports_spdy,
             const SettingsMap* settings_map,
             const AlternateProtocolInfo* alternate_protocol,
             const SupportsQuic* supports_quic,
             const ServerNetworkStats* server_network_stats)
      : supports_spdy(supports_spdy),
        settings_map(settings_map),
        alternate_protocol(alternate_protocol),
        supports_quic(supports_quic),
        server_network_stats(server_network_stats) {}
  bool supports_spdy;
  const SettingsMap* settings_map;
  const AlternateProtocolInfo* alternate_protocol;
  const SupportsQuic* supports_quic;
  const ServerNetworkStats* server_network_stats;
};

void HttpServerPropertiesManager::UpdatePrefsOnPrefThread(
    base::ListValue* spdy_server_list,
    SpdySettingsMap* spdy_settings_map,
    AlternateProtocolMap* alternate_protocol_map,
    IPAddressNumber* last_quic_address,
    ServerNetworkStatsMap* server_network_stats_map,
    const base::Closure& completion) {
  typedef std::map<HostPortPair, ServerPref> ServerPrefMap;
  ServerPrefMap server_pref_map;

  DCHECK(pref_task_runner_->RunsTasksOnCurrentThread());

  // Add servers that support spdy to server_pref_map.
  std::string s;
  for (base::ListValue::const_iterator list_it = spdy_server_list->begin();
       list_it != spdy_server_list->end();
       ++list_it) {
    if ((*list_it)->GetAsString(&s)) {
      HostPortPair server = HostPortPair::FromString(s);
      server_pref_map[server].supports_spdy = true;
    }
  }

  // Add servers that have SpdySettings to server_pref_map.
  for (SpdySettingsMap::iterator map_it = spdy_settings_map->begin();
       map_it != spdy_settings_map->end(); ++map_it) {
    const HostPortPair& server = map_it->first;
    server_pref_map[server].settings_map = &map_it->second;
  }

  // Add AlternateProtocol servers to server_pref_map.
  for (AlternateProtocolMap::const_iterator map_it =
           alternate_protocol_map->begin();
       map_it != alternate_protocol_map->end(); ++map_it) {
    server_pref_map[map_it->first].alternate_protocol = &map_it->second;
  }

  // Add ServerNetworkStats servers to server_pref_map.
  for (ServerNetworkStatsMap::const_iterator map_it =
           server_network_stats_map->begin();
       map_it != server_network_stats_map->end(); ++map_it) {
    const HostPortPair& server = map_it->first;
    server_pref_map[server].server_network_stats = &map_it->second;
  }

  // Persist properties to the |path_|.
  base::DictionaryValue http_server_properties_dict;
  base::DictionaryValue* servers_dict = new base::DictionaryValue;
  for (ServerPrefMap::const_iterator map_it = server_pref_map.begin();
       map_it != server_pref_map.end();
       ++map_it) {
    const HostPortPair& server = map_it->first;
    const ServerPref& server_pref = map_it->second;

    base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

    // Save supports_spdy.
    if (server_pref.supports_spdy)
      server_pref_dict->SetBoolean(kSupportsSpdyKey, server_pref.supports_spdy);
    SaveSpdySettingsToServerPrefs(server_pref.settings_map, server_pref_dict);
    SaveAlternateProtocolToServerPrefs(server_pref.alternate_protocol,
                                       server_pref_dict);
    SaveNetworkStatsToServerPrefs(server_pref.server_network_stats,
                                  server_pref_dict);

    servers_dict->SetWithoutPathExpansion(server.ToString(), server_pref_dict);
  }

  http_server_properties_dict.SetWithoutPathExpansion(kServersKey,
                                                      servers_dict);
  SetVersion(&http_server_properties_dict, kVersionNumber);

  SaveSupportsQuicToPrefs(last_quic_address, &http_server_properties_dict);

  setting_prefs_ = true;
  pref_service_->Set(path_, http_server_properties_dict);
  setting_prefs_ = false;

  // Note that |completion| will be fired after we have written everything to
  // the Preferences, but likely before these changes are serialized to disk.
  // This is not a problem though, as JSONPrefStore guarantees that this will
  // happen, pretty soon, and even in the case we shut down immediately.
  if (!completion.is_null())
    completion.Run();
}

void HttpServerPropertiesManager::SaveSpdySettingsToServerPrefs(
    const SettingsMap* settings_map,
    base::DictionaryValue* server_pref_dict) {
  if (!settings_map) {
    return;
  }
  base::DictionaryValue* spdy_settings_dict = new base::DictionaryValue;
  for (SettingsMap::const_iterator it = settings_map->begin();
       it != settings_map->end(); ++it) {
    SpdySettingsIds id = it->first;
    uint32 value = it->second.second;
    std::string key = base::StringPrintf("%u", id);
    spdy_settings_dict->SetInteger(key, value);
  }
  server_pref_dict->SetWithoutPathExpansion(kSettingsKey, spdy_settings_dict);
}

void HttpServerPropertiesManager::SaveAlternateProtocolToServerPrefs(
    const AlternateProtocolInfo* port_alternate_protocol,
    base::DictionaryValue* server_pref_dict) {
  if (!port_alternate_protocol)
    return;

  base::DictionaryValue* port_alternate_protocol_dict =
      new base::DictionaryValue;
  port_alternate_protocol_dict->SetInteger(kPortKey,
                                           port_alternate_protocol->port);
  const char* protocol_str =
      AlternateProtocolToString(port_alternate_protocol->protocol);
  port_alternate_protocol_dict->SetString(kProtocolKey, protocol_str);
  port_alternate_protocol_dict->SetDouble(kProbabilityKey,
                                          port_alternate_protocol->probability);
  server_pref_dict->SetWithoutPathExpansion(kAlternateProtocolKey,
                                            port_alternate_protocol_dict);
}

void HttpServerPropertiesManager::SaveSupportsQuicToPrefs(
    const IPAddressNumber* last_quic_address,
    base::DictionaryValue* http_server_properties_dict) {
  if (!last_quic_address || last_quic_address->empty())
    return;

  base::DictionaryValue* supports_quic_dict = new base::DictionaryValue;
  supports_quic_dict->SetBoolean(kUsedQuicKey, true);
  supports_quic_dict->SetString(kAddressKey,
                                IPAddressToString(*last_quic_address));
  http_server_properties_dict->SetWithoutPathExpansion(kSupportsQuicKey,
                                                       supports_quic_dict);
}

void HttpServerPropertiesManager::SaveNetworkStatsToServerPrefs(
    const ServerNetworkStats* server_network_stats,
    base::DictionaryValue* server_pref_dict) {
  if (!server_network_stats)
    return;

  base::DictionaryValue* server_network_stats_dict = new base::DictionaryValue;
  // Becasue JSON doesn't support int64, persist int64 as a string.
  server_network_stats_dict->SetInteger(
      kSrttKey, static_cast<int>(server_network_stats->srtt.ToInternalValue()));
  // TODO(rtenneti): When QUIC starts using bandwidth_estimate, then persist
  // bandwidth_estimate.
  server_pref_dict->SetWithoutPathExpansion(kNetworkStatsKey,
                                            server_network_stats_dict);
}

void HttpServerPropertiesManager::OnHttpServerPropertiesChanged() {
  DCHECK(pref_task_runner_->RunsTasksOnCurrentThread());
  if (!setting_prefs_)
    ScheduleUpdateCacheOnPrefThread();
}

}  // namespace net
