// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_server_properties_manager.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace chrome_browser_net {

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

// Load either 200 or 1000 servers based on a coin flip.
const int k200AlternateProtocolHostsToLoad = 200;
const int k1000AlternateProtocolHostsToLoad = 1000;
// Persist 1000 MRU AlternateProtocolHostPortPairs.
const int kMaxAlternateProtocolHostsToPersist = 1000;

// Persist 200 MRU SpdySettingsHostPortPairs.
const int kMaxSpdySettingsHostsToPersist = 200;

// Persist 300 MRU SupportsSpdyServerHostPortPairs.
const int kMaxSupportsSpdyServerHostsToPersist = 300;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  HttpServerPropertiesManager

HttpServerPropertiesManager::HttpServerPropertiesManager(
    PrefService* pref_service)
    : pref_service_(pref_service),
      setting_prefs_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(pref_service);
  ui_weak_ptr_factory_.reset(
      new base::WeakPtrFactory<HttpServerPropertiesManager>(this));
  ui_weak_ptr_ = ui_weak_ptr_factory_->GetWeakPtr();
  ui_cache_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kHttpServerProperties,
      base::Bind(&HttpServerPropertiesManager::OnHttpServerPropertiesChanged,
                 base::Unretained(this)));
}

HttpServerPropertiesManager::~HttpServerPropertiesManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_weak_ptr_factory_.reset();
}

void HttpServerPropertiesManager::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_weak_ptr_factory_.reset(
      new base::WeakPtrFactory<HttpServerPropertiesManager>(this));
  http_server_properties_impl_.reset(new net::HttpServerPropertiesImpl());

  io_prefs_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdateCacheFromPrefsOnUI,
                 ui_weak_ptr_));
}

void HttpServerPropertiesManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel any pending updates, and stop listening for pref change updates.
  ui_cache_update_timer_->Stop();
  ui_weak_ptr_factory_.reset();
  pref_change_registrar_.RemoveAll();
}

// static
void HttpServerPropertiesManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterDictionaryPref(
      prefs::kHttpServerProperties,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
void HttpServerPropertiesManager::SetVersion(
    base::DictionaryValue* http_server_properties_dict,
    int version_number) {
  if (version_number < 0)
    version_number =  kVersionNumber;
  DCHECK_LE(version_number, kVersionNumber);
  if (version_number <= kVersionNumber)
    http_server_properties_dict->SetInteger("version", version_number);
}

// This is required for conformance with the HttpServerProperties interface.
base::WeakPtr<net::HttpServerProperties>
    HttpServerPropertiesManager::GetWeakPtr() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return io_weak_ptr_factory_->GetWeakPtr();
}

void HttpServerPropertiesManager::Clear() {
  Clear(base::Closure());
}

void HttpServerPropertiesManager::Clear(const base::Closure& completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->Clear();
  UpdatePrefsFromCacheOnIO(completion);
}

bool HttpServerPropertiesManager::SupportsSpdy(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->SupportsSpdy(server);
}

void HttpServerPropertiesManager::SetSupportsSpdy(
    const net::HostPortPair& server,
    bool support_spdy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->SetSupportsSpdy(server, support_spdy);
  ScheduleUpdatePrefsOnIO();
}

bool HttpServerPropertiesManager::HasAlternateProtocol(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->HasAlternateProtocol(server);
}

net::PortAlternateProtocolPair
HttpServerPropertiesManager::GetAlternateProtocol(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->GetAlternateProtocol(server);
}

void HttpServerPropertiesManager::SetAlternateProtocol(
    const net::HostPortPair& server,
    uint16 alternate_port,
    net::AlternateProtocol alternate_protocol) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->SetAlternateProtocol(
      server, alternate_port, alternate_protocol);
  ScheduleUpdatePrefsOnIO();
}

void HttpServerPropertiesManager::SetBrokenAlternateProtocol(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->SetBrokenAlternateProtocol(server);
  ScheduleUpdatePrefsOnIO();
}

bool HttpServerPropertiesManager::WasAlternateProtocolRecentlyBroken(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->WasAlternateProtocolRecentlyBroken(
      server);
}

void HttpServerPropertiesManager::ConfirmAlternateProtocol(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->ConfirmAlternateProtocol(server);
  ScheduleUpdatePrefsOnIO();
}

void HttpServerPropertiesManager::ClearAlternateProtocol(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->ClearAlternateProtocol(server);
  ScheduleUpdatePrefsOnIO();
}

const net::AlternateProtocolMap&
HttpServerPropertiesManager::alternate_protocol_map() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->alternate_protocol_map();
}

void HttpServerPropertiesManager::SetAlternateProtocolExperiment(
    net::AlternateProtocolExperiment experiment) {
  http_server_properties_impl_->SetAlternateProtocolExperiment(experiment);
}

net::AlternateProtocolExperiment
HttpServerPropertiesManager::GetAlternateProtocolExperiment() const {
  return http_server_properties_impl_->GetAlternateProtocolExperiment();
}

const net::SettingsMap&
HttpServerPropertiesManager::GetSpdySettings(
    const net::HostPortPair& host_port_pair) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->GetSpdySettings(host_port_pair);
}

bool HttpServerPropertiesManager::SetSpdySetting(
    const net::HostPortPair& host_port_pair,
    net::SpdySettingsIds id,
    net::SpdySettingsFlags flags,
    uint32 value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool persist = http_server_properties_impl_->SetSpdySetting(
      host_port_pair, id, flags, value);
  if (persist)
    ScheduleUpdatePrefsOnIO();
  return persist;
}

void HttpServerPropertiesManager::ClearSpdySettings(
    const net::HostPortPair& host_port_pair) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->ClearSpdySettings(host_port_pair);
  ScheduleUpdatePrefsOnIO();
}

void HttpServerPropertiesManager::ClearAllSpdySettings() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->ClearAllSpdySettings();
  ScheduleUpdatePrefsOnIO();
}

const net::SpdySettingsMap&
HttpServerPropertiesManager::spdy_settings_map() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->spdy_settings_map();
}

void HttpServerPropertiesManager::SetServerNetworkStats(
    const net::HostPortPair& host_port_pair,
    NetworkStats stats) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->SetServerNetworkStats(host_port_pair, stats);
}

const HttpServerPropertiesManager::NetworkStats*
HttpServerPropertiesManager::GetServerNetworkStats(
    const net::HostPortPair& host_port_pair) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->GetServerNetworkStats(host_port_pair);
}

//
// Update the HttpServerPropertiesImpl's cache with data from preferences.
//
void HttpServerPropertiesManager::ScheduleUpdateCacheOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel pending updates, if any.
  ui_cache_update_timer_->Stop();
  StartCacheUpdateTimerOnUI(
      base::TimeDelta::FromMilliseconds(kUpdateCacheDelayMs));
}

void HttpServerPropertiesManager::StartCacheUpdateTimerOnUI(
    base::TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui_cache_update_timer_->Start(
      FROM_HERE, delay, this,
      &HttpServerPropertiesManager::UpdateCacheFromPrefsOnUI);
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnUI() {
  // The preferences can only be read on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!pref_service_->HasPrefPath(prefs::kHttpServerProperties))
    return;

  bool detected_corrupted_prefs = false;
  const base::DictionaryValue& http_server_properties_dict =
      *pref_service_->GetDictionary(prefs::kHttpServerProperties);

  int version = kMissingVersion;
  if (!http_server_properties_dict.GetIntegerWithoutPathExpansion(
      "version", &version)) {
    DVLOG(1) << "Missing version. Clearing all properties.";
    return;
  }

  // The properties for a given server is in
  // http_server_properties_dict["servers"][server].
  const base::DictionaryValue* servers_dict = NULL;
  if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
      "servers", &servers_dict)) {
    DVLOG(1) << "Malformed http_server_properties for servers.";
    return;
  }

  // String is host/port pair of spdy server.
  scoped_ptr<StringVector> spdy_servers(new StringVector);
  scoped_ptr<net::SpdySettingsMap> spdy_settings_map(
      new net::SpdySettingsMap(kMaxSpdySettingsHostsToPersist));
  scoped_ptr<net::AlternateProtocolMap> alternate_protocol_map(
      new net::AlternateProtocolMap(kMaxAlternateProtocolHostsToPersist));
  // TODO(rtenneti): Delete the following code after the experiment.
  int alternate_protocols_to_load = k200AlternateProtocolHostsToLoad;
  net::AlternateProtocolExperiment alternate_protocol_experiment =
      net::ALTERNATE_PROTOCOL_NOT_PART_OF_EXPERIMENT;
  if (version == kVersionNumber) {
    if (base::RandInt(0, 99) == 0) {
      alternate_protocol_experiment =
          net::ALTERNATE_PROTOCOL_TRUNCATED_200_SERVERS;
    } else {
      alternate_protocols_to_load = k1000AlternateProtocolHostsToLoad;
      alternate_protocol_experiment =
          net::ALTERNATE_PROTOCOL_TRUNCATED_1000_SERVERS;
    }
    DVLOG(1) << "# of servers that support alternate_protocol: "
             << alternate_protocols_to_load;
  }

  int count = 0;
  for (base::DictionaryValue::Iterator it(*servers_dict); !it.IsAtEnd();
       it.Advance()) {
    // Get server's host/pair.
    const std::string& server_str = it.key();
    net::HostPortPair server = net::HostPortPair::FromString(server_str);
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
    if ((server_pref_dict->GetBoolean(
         "supports_spdy", &supports_spdy)) && supports_spdy) {
      spdy_servers->push_back(server_str);
    }

    // Get SpdySettings.
    DCHECK(spdy_settings_map->Peek(server) == spdy_settings_map->end());
    const base::DictionaryValue* spdy_settings_dict = NULL;
    if (server_pref_dict->GetDictionaryWithoutPathExpansion(
        "settings", &spdy_settings_dict)) {
      net::SettingsMap settings_map;
      for (base::DictionaryValue::Iterator dict_it(*spdy_settings_dict);
           !dict_it.IsAtEnd(); dict_it.Advance()) {
        const std::string& id_str = dict_it.key();
        int id = 0;
        if (!base::StringToInt(id_str, &id)) {
          DVLOG(1) << "Malformed id in SpdySettings for server: " <<
              server_str;
          NOTREACHED();
          continue;
        }
        int value = 0;
        if (!dict_it.value().GetAsInteger(&value)) {
          DVLOG(1) << "Malformed value in SpdySettings for server: " <<
              server_str;
          NOTREACHED();
          continue;
        }
        net::SettingsFlagsAndValue flags_and_value(
            net::SETTINGS_FLAG_PERSISTED, value);
        settings_map[static_cast<net::SpdySettingsIds>(id)] = flags_and_value;
      }
      spdy_settings_map->Put(server, settings_map);
    }

    // Get alternate_protocol server.
    DCHECK(alternate_protocol_map->Peek(server) ==
           alternate_protocol_map->end());
    const base::DictionaryValue* port_alternate_protocol_dict = NULL;
    if (!server_pref_dict->GetDictionaryWithoutPathExpansion(
        "alternate_protocol", &port_alternate_protocol_dict)) {
      continue;
    }

    if (count >= alternate_protocols_to_load)
      continue;
    do {
      int port = 0;
      if (!port_alternate_protocol_dict->GetIntegerWithoutPathExpansion(
          "port", &port) || (port > (1 << 16))) {
        DVLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
        detected_corrupted_prefs = true;
        continue;
      }
      std::string protocol_str;
      if (!port_alternate_protocol_dict->GetStringWithoutPathExpansion(
              "protocol_str", &protocol_str)) {
        DVLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
        detected_corrupted_prefs = true;
        continue;
      }
      net::AlternateProtocol protocol =
          net::AlternateProtocolFromString(protocol_str);
      if (!net::IsAlternateProtocolValid(protocol)) {
        DVLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
        detected_corrupted_prefs = true;
        continue;
      }

      net::PortAlternateProtocolPair port_alternate_protocol;
      port_alternate_protocol.port = port;
      port_alternate_protocol.protocol = protocol;

      alternate_protocol_map->Put(server, port_alternate_protocol);
      ++count;
    } while (false);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::
                 UpdateCacheFromPrefsOnIO,
                 base::Unretained(this),
                 base::Owned(spdy_servers.release()),
                 base::Owned(spdy_settings_map.release()),
                 base::Owned(alternate_protocol_map.release()),
                 alternate_protocol_experiment,
                 detected_corrupted_prefs));
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnIO(
    StringVector* spdy_servers,
    net::SpdySettingsMap* spdy_settings_map,
    net::AlternateProtocolMap* alternate_protocol_map,
    net::AlternateProtocolExperiment alternate_protocol_experiment,
    bool detected_corrupted_prefs) {
  // Preferences have the master data because admins might have pushed new
  // preferences. Update the cached data with new data from preferences.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

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
  http_server_properties_impl_->SetAlternateProtocolExperiment(
      alternate_protocol_experiment);

  // Update the prefs with what we have read (delete all corrupted prefs).
  if (detected_corrupted_prefs)
    ScheduleUpdatePrefsOnIO();
}


//
// Update Preferences with data from the cached data.
//
void HttpServerPropertiesManager::ScheduleUpdatePrefsOnIO() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Cancel pending updates, if any.
  io_prefs_update_timer_->Stop();
  StartPrefsUpdateTimerOnIO(
      base::TimeDelta::FromMilliseconds(kUpdatePrefsDelayMs));
}

void HttpServerPropertiesManager::StartPrefsUpdateTimerOnIO(
    base::TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // This is overridden in tests to post the task without the delay.
  io_prefs_update_timer_->Start(
      FROM_HERE, delay, this,
      &HttpServerPropertiesManager::UpdatePrefsFromCacheOnIO);
}

// This is required so we can set this as the callback for a timer.
void HttpServerPropertiesManager::UpdatePrefsFromCacheOnIO() {
  UpdatePrefsFromCacheOnIO(base::Closure());
}

void HttpServerPropertiesManager::UpdatePrefsFromCacheOnIO(
    const base::Closure& completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::ListValue* spdy_server_list = new base::ListValue;
  http_server_properties_impl_->GetSpdyServerList(
      spdy_server_list, kMaxSupportsSpdyServerHostsToPersist);

  net::SpdySettingsMap* spdy_settings_map =
      new net::SpdySettingsMap(kMaxSpdySettingsHostsToPersist);
  const net::SpdySettingsMap& main_map =
      http_server_properties_impl_->spdy_settings_map();
  int count = 0;
  for (net::SpdySettingsMap::const_iterator it = main_map.begin();
       it != main_map.end() && count < kMaxSpdySettingsHostsToPersist;
       ++it, ++count) {
    spdy_settings_map->Put(it->first, it->second);
  }

  net::AlternateProtocolMap* alternate_protocol_map =
      new net::AlternateProtocolMap(kMaxAlternateProtocolHostsToPersist);
  const net::AlternateProtocolMap& map =
      http_server_properties_impl_->alternate_protocol_map();
  count = 0;
  typedef std::map<std::string, bool> CanonicalHostPersistedMap;
  CanonicalHostPersistedMap persisted_map;
  for (net::AlternateProtocolMap::const_iterator it = map.begin();
       it != map.end() && count < kMaxAlternateProtocolHostsToPersist;
       ++it) {
    const net::HostPortPair& server = it->first;
    std::string canonical_suffix =
        http_server_properties_impl_->GetCanonicalSuffix(server);
    if (!canonical_suffix.empty()) {
      if (persisted_map.find(canonical_suffix) != persisted_map.end())
        continue;
      persisted_map[canonical_suffix] = true;
    }
    alternate_protocol_map->Put(server, it->second);
    ++count;
  }

  // Update the preferences on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdatePrefsOnUI,
                 ui_weak_ptr_,
                 base::Owned(spdy_server_list),
                 base::Owned(spdy_settings_map),
                 base::Owned(alternate_protocol_map),
                 completion));
}

// A local or temporary data structure to hold |supports_spdy|, SpdySettings,
// and PortAlternateProtocolPair preferences for a server. This is used only in
// UpdatePrefsOnUI.
struct ServerPref {
  ServerPref()
      : supports_spdy(false),
        settings_map(NULL),
        alternate_protocol(NULL) {
  }
  ServerPref(bool supports_spdy,
             const net::SettingsMap* settings_map,
             const net::PortAlternateProtocolPair* alternate_protocol)
      : supports_spdy(supports_spdy),
        settings_map(settings_map),
        alternate_protocol(alternate_protocol) {
  }
  bool supports_spdy;
  const net::SettingsMap* settings_map;
  const net::PortAlternateProtocolPair* alternate_protocol;
};

void HttpServerPropertiesManager::UpdatePrefsOnUI(
    base::ListValue* spdy_server_list,
    net::SpdySettingsMap* spdy_settings_map,
    net::AlternateProtocolMap* alternate_protocol_map,
    const base::Closure& completion) {

  typedef std::map<net::HostPortPair, ServerPref> ServerPrefMap;
  ServerPrefMap server_pref_map;

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Add servers that support spdy to server_pref_map.
  std::string s;
  for (base::ListValue::const_iterator list_it = spdy_server_list->begin();
       list_it != spdy_server_list->end(); ++list_it) {
    if ((*list_it)->GetAsString(&s)) {
      net::HostPortPair server = net::HostPortPair::FromString(s);

      ServerPrefMap::iterator it = server_pref_map.find(server);
      if (it == server_pref_map.end()) {
        ServerPref server_pref(true, NULL, NULL);
        server_pref_map[server] = server_pref;
      } else {
        it->second.supports_spdy = true;
      }
    }
  }

  // Add servers that have SpdySettings to server_pref_map.
  for (net::SpdySettingsMap::iterator map_it = spdy_settings_map->begin();
       map_it != spdy_settings_map->end(); ++map_it) {
    const net::HostPortPair& server = map_it->first;

    ServerPrefMap::iterator it = server_pref_map.find(server);
    if (it == server_pref_map.end()) {
      ServerPref server_pref(false, &map_it->second, NULL);
      server_pref_map[server] = server_pref;
    } else {
      it->second.settings_map = &map_it->second;
    }
  }

  // Add AlternateProtocol servers to server_pref_map.
  for (net::AlternateProtocolMap::const_iterator map_it =
           alternate_protocol_map->begin();
       map_it != alternate_protocol_map->end(); ++map_it) {
    const net::HostPortPair& server = map_it->first;
    const net::PortAlternateProtocolPair& port_alternate_protocol =
        map_it->second;
    if (!net::IsAlternateProtocolValid(port_alternate_protocol.protocol)) {
      continue;
    }

    ServerPrefMap::iterator it = server_pref_map.find(server);
    if (it == server_pref_map.end()) {
      ServerPref server_pref(false, NULL, &map_it->second);
      server_pref_map[server] = server_pref;
    } else {
      it->second.alternate_protocol = &map_it->second;
    }
  }

  // Persist the prefs::kHttpServerProperties.
  base::DictionaryValue http_server_properties_dict;
  base::DictionaryValue* servers_dict = new base::DictionaryValue;
  for (ServerPrefMap::const_iterator map_it =
       server_pref_map.begin();
       map_it != server_pref_map.end(); ++map_it) {
    const net::HostPortPair& server = map_it->first;
    const ServerPref& server_pref = map_it->second;

    base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

    // Save supports_spdy.
    if (server_pref.supports_spdy)
      server_pref_dict->SetBoolean("supports_spdy", server_pref.supports_spdy);

    // Save SPDY settings.
    if (server_pref.settings_map) {
      base::DictionaryValue* spdy_settings_dict = new base::DictionaryValue;
      for (net::SettingsMap::const_iterator it =
           server_pref.settings_map->begin();
           it != server_pref.settings_map->end(); ++it) {
        net::SpdySettingsIds id = it->first;
        uint32 value = it->second.second;
        std::string key = base::StringPrintf("%u", id);
        spdy_settings_dict->SetInteger(key, value);
      }
      server_pref_dict->SetWithoutPathExpansion("settings", spdy_settings_dict);
    }

    // Save alternate_protocol.
    if (server_pref.alternate_protocol) {
      base::DictionaryValue* port_alternate_protocol_dict =
          new base::DictionaryValue;
      const net::PortAlternateProtocolPair* port_alternate_protocol =
          server_pref.alternate_protocol;
      port_alternate_protocol_dict->SetInteger(
          "port", port_alternate_protocol->port);
      const char* protocol_str =
          net::AlternateProtocolToString(port_alternate_protocol->protocol);
      port_alternate_protocol_dict->SetString("protocol_str", protocol_str);
      server_pref_dict->SetWithoutPathExpansion(
          "alternate_protocol", port_alternate_protocol_dict);
    }

    servers_dict->SetWithoutPathExpansion(server.ToString(), server_pref_dict);
  }

  http_server_properties_dict.SetWithoutPathExpansion("servers", servers_dict);
  SetVersion(&http_server_properties_dict, kVersionNumber);
  setting_prefs_ = true;
  pref_service_->Set(prefs::kHttpServerProperties,
                     http_server_properties_dict);
  setting_prefs_ = false;

  // Note that |completion| will be fired after we have written everything to
  // the Preferences, but likely before these changes are serialized to disk.
  // This is not a problem though, as JSONPrefStore guarantees that this will
  // happen, pretty soon, and even in the case we shut down immediately.
  if (!completion.is_null())
    completion.Run();
}

void HttpServerPropertiesManager::OnHttpServerPropertiesChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!setting_prefs_)
    ScheduleUpdateCacheOnUI();
}

}  // namespace chrome_browser_net
