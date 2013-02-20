// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/net/http_server_properties_manager.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
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
const int kVersionNumber = 1;

typedef std::vector<std::string> StringVector;

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
}

void HttpServerPropertiesManager::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
void HttpServerPropertiesManager::RegisterUserPrefs(
    PrefRegistrySyncable* prefs) {
  prefs->RegisterDictionaryPref(prefs::kHttpServerProperties,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// This is required for conformance with the HttpServerProperties interface.
void HttpServerPropertiesManager::Clear() {
  Clear(base::Closure());
}

void HttpServerPropertiesManager::Clear(const base::Closure& completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->Clear();
  UpdatePrefsFromCacheOnIO(completion);
}

bool HttpServerPropertiesManager::SupportsSpdy(
    const net::HostPortPair& server) const {
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
    const net::HostPortPair& server) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->HasAlternateProtocol(server);
}

net::PortAlternateProtocolPair
HttpServerPropertiesManager::GetAlternateProtocol(
    const net::HostPortPair& server) const {
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

const net::AlternateProtocolMap&
HttpServerPropertiesManager::alternate_protocol_map() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->alternate_protocol_map();
}

const net::SettingsMap&
HttpServerPropertiesManager::GetSpdySettings(
    const net::HostPortPair& host_port_pair) const {
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

void HttpServerPropertiesManager::ClearSpdySettings() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->ClearSpdySettings();
  ScheduleUpdatePrefsOnIO();
}

const net::SpdySettingsMap&
HttpServerPropertiesManager::spdy_settings_map() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->spdy_settings_map();
}

net::HttpPipelinedHostCapability
HttpServerPropertiesManager::GetPipelineCapability(
    const net::HostPortPair& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->GetPipelineCapability(origin);
}

void HttpServerPropertiesManager::SetPipelineCapability(
    const net::HostPortPair& origin,
    net::HttpPipelinedHostCapability capability) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->SetPipelineCapability(origin, capability);
  ScheduleUpdatePrefsOnIO();
}

void HttpServerPropertiesManager::ClearPipelineCapabilities() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->ClearPipelineCapabilities();
  ScheduleUpdatePrefsOnIO();
}

net::PipelineCapabilityMap
HttpServerPropertiesManager::GetPipelineCapabilityMap() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->GetPipelineCapabilityMap();
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

  // Initialize version to kMissingVersion because there might not be a
  // "version" key in the properties.
  int version = kMissingVersion;
  http_server_properties_dict.GetIntegerWithoutPathExpansion(
      "version", &version);

  const base::DictionaryValue* servers_dict;
  if (version == kMissingVersion) {
    // If http_server_properties_dict has no "version" key and no "servers" key,
    // then the properties for a given server are in
    // http_server_properties_dict[server].
    servers_dict = &http_server_properties_dict;
  } else {
    // The "new" format has "version" and "servers" keys. The properties for a
    // given server is in http_server_properties_dict["servers"][server].
    const base::DictionaryValue* servers_dict_temp = NULL;
    if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
        "servers", &servers_dict_temp)) {
      DVLOG(1) << "Malformed http_server_properties for servers";
      return;
    }
    servers_dict = servers_dict_temp;
  }

  // String is host/port pair of spdy server.
  scoped_ptr<StringVector> spdy_servers(new StringVector);
  scoped_ptr<net::SpdySettingsMap> spdy_settings_map(new net::SpdySettingsMap);
  scoped_ptr<net::PipelineCapabilityMap> pipeline_capability_map(
      new net::PipelineCapabilityMap);
  scoped_ptr<net::AlternateProtocolMap> alternate_protocol_map(
      new net::AlternateProtocolMap);

  for (base::DictionaryValue::key_iterator it = servers_dict->begin_keys();
       it != servers_dict->end_keys();
       ++it) {
    // Get server's host/pair.
    const std::string& server_str = *it;
    net::HostPortPair server = net::HostPortPair::FromString(server_str);
    if (server.host().empty()) {
      DVLOG(1) << "Malformed http_server_properties for server: " << server_str;
      detected_corrupted_prefs = true;
      continue;
    }

    const base::DictionaryValue* server_pref_dict = NULL;
    if (!servers_dict->GetDictionaryWithoutPathExpansion(
        server_str, &server_pref_dict)) {
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
    DCHECK(!ContainsKey(*spdy_settings_map, server));
    if (version == kVersionNumber) {
      const base::DictionaryValue* spdy_settings_dict = NULL;
      if (server_pref_dict->GetDictionaryWithoutPathExpansion(
          "settings", &spdy_settings_dict)) {
        net::SettingsMap settings_map;
        for (base::DictionaryValue::key_iterator dict_it =
             spdy_settings_dict->begin_keys();
             dict_it != spdy_settings_dict->end_keys(); ++dict_it) {
          const std::string& id_str = *dict_it;
          int id = 0;
          if (!base::StringToInt(id_str, &id)) {
            DVLOG(1) << "Malformed id in SpdySettings for server: " <<
                server_str;
            NOTREACHED();
            continue;
          }
          int value = 0;
          if (!spdy_settings_dict->GetIntegerWithoutPathExpansion(id_str,
                                                                  &value)) {
            DVLOG(1) << "Malformed value in SpdySettings for server: " <<
                server_str;
            NOTREACHED();
            continue;
          }
          net::SettingsFlagsAndValue flags_and_value(
              net::SETTINGS_FLAG_PERSISTED, value);
          settings_map[static_cast<net::SpdySettingsIds>(id)] = flags_and_value;
        }
        (*spdy_settings_map)[server] = settings_map;
      }
    }

    int pipeline_capability = net::PIPELINE_UNKNOWN;
    if ((server_pref_dict->GetInteger(
         "pipeline_capability", &pipeline_capability)) &&
        pipeline_capability != net::PIPELINE_UNKNOWN) {
      (*pipeline_capability_map)[server] =
          static_cast<net::HttpPipelinedHostCapability>(pipeline_capability);
    }

    // Get alternate_protocol server.
    DCHECK(!ContainsKey(*alternate_protocol_map, server));
    const base::DictionaryValue* port_alternate_protocol_dict = NULL;
    if (!server_pref_dict->GetDictionaryWithoutPathExpansion(
        "alternate_protocol", &port_alternate_protocol_dict)) {
      continue;
    }

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
      if (protocol > net::NUM_ALTERNATE_PROTOCOLS) {
        DVLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
        detected_corrupted_prefs = true;
        continue;
      }

      net::PortAlternateProtocolPair port_alternate_protocol;
      port_alternate_protocol.port = port;
      port_alternate_protocol.protocol = protocol;

      (*alternate_protocol_map)[server] = port_alternate_protocol;
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
                 base::Owned(pipeline_capability_map.release()),
                 detected_corrupted_prefs));
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnIO(
    StringVector* spdy_servers,
    net::SpdySettingsMap* spdy_settings_map,
    net::AlternateProtocolMap* alternate_protocol_map,
    net::PipelineCapabilityMap* pipeline_capability_map,
    bool detected_corrupted_prefs) {
  // Preferences have the master data because admins might have pushed new
  // preferences. Update the cached data with new data from preferences.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  UMA_HISTOGRAM_COUNTS("Net.CountOfSpdyServers", spdy_servers->size());
  http_server_properties_impl_->InitializeSpdyServers(spdy_servers, true);

  // Clear the cached data and use the new spdy_settings from preferences.
  UMA_HISTOGRAM_COUNTS("Net.CountOfSpdySettings", spdy_settings_map->size());
  http_server_properties_impl_->InitializeSpdySettingsServers(
      spdy_settings_map);

  // Clear the cached data and use the new Alternate-Protocol server list from
  // preferences.
  UMA_HISTOGRAM_COUNTS("Net.CountOfAlternateProtocolServers",
                       alternate_protocol_map->size());
  http_server_properties_impl_->InitializeAlternateProtocolServers(
      alternate_protocol_map);

  UMA_HISTOGRAM_COUNTS("Net.CountOfPipelineCapableServers",
                       pipeline_capability_map->size());
  http_server_properties_impl_->InitializePipelineCapabilities(
      pipeline_capability_map);

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
  http_server_properties_impl_->GetSpdyServerList(spdy_server_list);

  net::SpdySettingsMap* spdy_settings_map = new net::SpdySettingsMap;
  *spdy_settings_map = http_server_properties_impl_->spdy_settings_map();

  net::AlternateProtocolMap* alternate_protocol_map =
      new net::AlternateProtocolMap;
  *alternate_protocol_map =
      http_server_properties_impl_->alternate_protocol_map();

  net::PipelineCapabilityMap* pipeline_capability_map =
      new net::PipelineCapabilityMap;
  *pipeline_capability_map =
      http_server_properties_impl_->GetPipelineCapabilityMap();

  // Update the preferences on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdatePrefsOnUI,
                 ui_weak_ptr_,
                 base::Owned(spdy_server_list),
                 base::Owned(spdy_settings_map),
                 base::Owned(alternate_protocol_map),
                 base::Owned(pipeline_capability_map),
                 completion));
}

// A local or temporary data structure to hold |supports_spdy|, SpdySettings,
// PortAlternateProtocolPair, and |pipeline_capability| preferences for a
// server. This is used only in UpdatePrefsOnUI.
struct ServerPref {
  ServerPref()
      : supports_spdy(false),
        settings_map(NULL),
        alternate_protocol(NULL),
        pipeline_capability(net::PIPELINE_UNKNOWN) {
  }
  ServerPref(bool supports_spdy,
             const net::SettingsMap* settings_map,
             const net::PortAlternateProtocolPair* alternate_protocol)
      : supports_spdy(supports_spdy),
        settings_map(settings_map),
        alternate_protocol(alternate_protocol),
        pipeline_capability(net::PIPELINE_UNKNOWN) {
  }
  bool supports_spdy;
  const net::SettingsMap* settings_map;
  const net::PortAlternateProtocolPair* alternate_protocol;
  net::HttpPipelinedHostCapability pipeline_capability;
};

void HttpServerPropertiesManager::UpdatePrefsOnUI(
    base::ListValue* spdy_server_list,
    net::SpdySettingsMap* spdy_settings_map,
    net::AlternateProtocolMap* alternate_protocol_map,
    net::PipelineCapabilityMap* pipeline_capability_map,
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
  for (net::SpdySettingsMap::iterator map_it =
       spdy_settings_map->begin();
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
    if (port_alternate_protocol.protocol < 0 ||
        port_alternate_protocol.protocol >= net::NUM_ALTERNATE_PROTOCOLS) {
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

  for (net::PipelineCapabilityMap::const_iterator map_it =
           pipeline_capability_map->begin();
       map_it != pipeline_capability_map->end(); ++map_it) {
    const net::HostPortPair& server = map_it->first;
    const net::HttpPipelinedHostCapability& pipeline_capability =
        map_it->second;

    ServerPrefMap::iterator it = server_pref_map.find(server);
    if (it == server_pref_map.end()) {
      ServerPref server_pref;
      server_pref.pipeline_capability = pipeline_capability;
      server_pref_map[server] = server_pref;
    } else {
      it->second.pipeline_capability = pipeline_capability;
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

    if (server_pref.pipeline_capability != net::PIPELINE_UNKNOWN) {
      server_pref_dict->SetInteger("pipeline_capability",
                                   server_pref.pipeline_capability);
    }

    servers_dict->SetWithoutPathExpansion(server.ToString(), server_pref_dict);
  }

  http_server_properties_dict.SetWithoutPathExpansion("servers", servers_dict);
  http_server_properties_dict.SetInteger("version", kVersionNumber);
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
