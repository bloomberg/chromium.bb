// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/net/http_server_properties_manager.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
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
  pref_change_registrar_.Add(prefs::kHttpServerProperties, this);
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
void HttpServerPropertiesManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kHttpServerProperties,
                                PrefService::UNSYNCABLE_PREF);
}

void HttpServerPropertiesManager::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->Clear();
  ScheduleUpdatePrefsOnIO();
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

const spdy::SpdySettings&
HttpServerPropertiesManager::GetSpdySettings(
    const net::HostPortPair& host_port_pair) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->GetSpdySettings(host_port_pair);
}

// Saves settings for a host.
bool HttpServerPropertiesManager::SetSpdySettings(
    const net::HostPortPair& host_port_pair,
    const spdy::SpdySettings& settings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool persist = http_server_properties_impl_->SetSpdySettings(
      host_port_pair, settings);
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

  // String is host/port pair of spdy server.
  StringVector* spdy_servers = new StringVector;

  // Parse the preferences into a SpdySettingsMap.
  net::SpdySettingsMap* spdy_settings_map = new net::SpdySettingsMap;

  // Parse the preferences into a AlternateProtocolMap.
  net::AlternateProtocolMap* alternate_protocol_map =
      new net::AlternateProtocolMap;

  const base::DictionaryValue& http_server_properties_dict =
      *pref_service_->GetDictionary(prefs::kHttpServerProperties);
  for (base::DictionaryValue::key_iterator it =
       http_server_properties_dict.begin_keys();
       it != http_server_properties_dict.end_keys(); ++it) {
    // Get server's host/pair.
    const std::string& server_str = *it;
    net::HostPortPair server = net::HostPortPair::FromString(server_str);
    if (server.host().empty()) {
      DVLOG(1) << "Malformed http_server_properties for server: " << server_str;
      NOTREACHED();
      continue;
    }

    base::DictionaryValue* server_pref_dict = NULL;
    if (!http_server_properties_dict.GetDictionaryWithoutPathExpansion(
        server_str, &server_pref_dict)) {
      DVLOG(1) << "Malformed http_server_properties server: " << server_str;
      NOTREACHED();
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
    base::ListValue* spdy_settings_list = NULL;
    if (server_pref_dict->GetListWithoutPathExpansion(
        "settings", &spdy_settings_list)) {
      spdy::SpdySettings spdy_settings;

      for (base::ListValue::const_iterator list_it =
           spdy_settings_list->begin();
           list_it != spdy_settings_list->end(); ++list_it) {
        if ((*list_it)->GetType() != Value::TYPE_DICTIONARY) {
          DVLOG(1) << "Malformed SpdySettingsList for server: " << server_str;
          NOTREACHED();
          continue;
        }

        const base::DictionaryValue* spdy_setting_dict =
            static_cast<const base::DictionaryValue*>(*list_it);

        int id = 0;
        if (!spdy_setting_dict->GetIntegerWithoutPathExpansion("id", &id)) {
          DVLOG(1) << "Malformed id in SpdySettings for server: " << server_str;
          NOTREACHED();
          continue;
        }

        int value = 0;
        if (!spdy_setting_dict->GetIntegerWithoutPathExpansion("value",
                                                               &value)) {
          DVLOG(1) << "Malformed value in SpdySettings for server: " <<
              server_str;
          NOTREACHED();
          continue;
        }

        spdy::SettingsFlagsAndId flags_and_id(0);
        flags_and_id.set_id(id);
        flags_and_id.set_flags(spdy::SETTINGS_FLAG_PERSISTED);

        spdy_settings.push_back(spdy::SpdySetting(flags_and_id, value));
      }

      (*spdy_settings_map)[server] = spdy_settings;
    }

    // Get alternate_protocol server.
    DCHECK(!ContainsKey(*alternate_protocol_map, server));
    base::DictionaryValue* port_alternate_protocol_dict = NULL;
    if (!server_pref_dict->GetDictionaryWithoutPathExpansion(
        "alternate_protocol", &port_alternate_protocol_dict)) {
      continue;
    }

    do {
      int port = 0;
      if (!port_alternate_protocol_dict->GetIntegerWithoutPathExpansion(
          "port", &port) || (port > (1 << 16))) {
        DVLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
        NOTREACHED();
        continue;
      }
      int protocol = 0;
      if (!port_alternate_protocol_dict->GetIntegerWithoutPathExpansion(
          "protocol", &protocol) || (protocol < 0) ||
          (protocol > net::NUM_ALTERNATE_PROTOCOLS)) {
        DVLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
        NOTREACHED();
        continue;
      }

      net::PortAlternateProtocolPair port_alternate_protocol;
      port_alternate_protocol.port = port;
      port_alternate_protocol.protocol = static_cast<net::AlternateProtocol>(
          protocol);

      (*alternate_protocol_map)[server] = port_alternate_protocol;
    } while (false);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::
                 UpdateCacheFromPrefsOnIO,
                 base::Unretained(this),
                 base::Owned(spdy_servers),
                 base::Owned(spdy_settings_map),
                 base::Owned(alternate_protocol_map)));
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnIO(
    StringVector* spdy_servers,
    net::SpdySettingsMap* spdy_settings_map,
    net::AlternateProtocolMap* alternate_protocol_map) {
  // Preferences have the master data because admins might have pushed new
  // preferences. Update the cached data with new data from preferences.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->InitializeSpdyServers(spdy_servers, true);

  // Clear the cached data and use the new spdy_settings from preferences.
  http_server_properties_impl_->InitializeSpdySettingsServers(
      spdy_settings_map);

  // Clear the cached data and use the new Alternate-Protocol server list from
  // preferences.
  http_server_properties_impl_->InitializeAlternateProtocolServers(
      alternate_protocol_map);
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

void HttpServerPropertiesManager::UpdatePrefsFromCacheOnIO() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  base::ListValue* spdy_server_list = new base::ListValue;
  http_server_properties_impl_->GetSpdyServerList(spdy_server_list);

  net::SpdySettingsMap* spdy_settings_map = new net::SpdySettingsMap;
  *spdy_settings_map = http_server_properties_impl_->spdy_settings_map();

  net::AlternateProtocolMap* alternate_protocol_map =
      new net::AlternateProtocolMap;
  *alternate_protocol_map =
      http_server_properties_impl_->alternate_protocol_map();

  // Update the preferences on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdatePrefsOnUI,
                 ui_weak_ptr_,
                 base::Owned(spdy_server_list),
                 base::Owned(spdy_settings_map),
                 base::Owned(alternate_protocol_map)));
}

// A local or temporary data structure to hold supports_spdy, SpdySettings and
// PortAlternateProtocolPair preferences for a server. This is used only in
// UpdatePrefsOnUI.
struct ServerPref {
  ServerPref()
      : supports_spdy(false),
        settings(NULL),
        alternate_protocol(NULL) {
  }
  ServerPref(bool supports_spdy,
             const spdy::SpdySettings* settings,
             const net::PortAlternateProtocolPair* alternate_protocol)
      : supports_spdy(supports_spdy),
        settings(settings),
        alternate_protocol(alternate_protocol) {
  }
  bool supports_spdy;
  const spdy::SpdySettings* settings;
  const net::PortAlternateProtocolPair* alternate_protocol;
};

void HttpServerPropertiesManager::UpdatePrefsOnUI(
    base::ListValue* spdy_server_list,
    net::SpdySettingsMap* spdy_settings_map,
    net::AlternateProtocolMap* alternate_protocol_map) {

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
      it->second.settings = &map_it->second;
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

  // Persist the prefs::kHttpServerProperties.
  base::DictionaryValue http_server_properties_dict;
  for (ServerPrefMap::const_iterator map_it =
       server_pref_map.begin();
       map_it != server_pref_map.end(); ++map_it) {
    const net::HostPortPair& server = map_it->first;
    const ServerPref& server_pref = map_it->second;

    base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

    // Save supports_spdy.
    server_pref_dict->SetBoolean("supports_spdy", server_pref.supports_spdy);

    // Save SpdySettings.
    if (server_pref.settings) {
      base::ListValue* spdy_settings_list = new ListValue();
      for (spdy::SpdySettings::const_iterator it =
           server_pref.settings->begin();
           it != server_pref.settings->end(); ++it) {
        uint32 id = it->first.id();
        uint32 value = it->second;
        base::DictionaryValue* spdy_setting_dict = new base::DictionaryValue;
        spdy_setting_dict->SetInteger("id", id);
        spdy_setting_dict->SetInteger("value", value);
        spdy_settings_list->Append(spdy_setting_dict);
      }
      server_pref_dict->Set("settings", spdy_settings_list);
    }

    // Save alternate_protocol.
    if (server_pref.alternate_protocol) {
      base::DictionaryValue* port_alternate_protocol_dict =
          new base::DictionaryValue;
      const net::PortAlternateProtocolPair* port_alternate_protocol =
          server_pref.alternate_protocol;
      port_alternate_protocol_dict->SetInteger(
          "port", port_alternate_protocol->port);
      port_alternate_protocol_dict->SetInteger(
          "protocol", port_alternate_protocol->protocol);
      server_pref_dict->SetWithoutPathExpansion(
          "alternate_protocol", port_alternate_protocol_dict);
    }
    http_server_properties_dict.SetWithoutPathExpansion(
          server.ToString(), server_pref_dict);
  }

  setting_prefs_ = true;
  pref_service_->Set(prefs::kHttpServerProperties,
                     http_server_properties_dict);
  setting_prefs_ = false;
}

void HttpServerPropertiesManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  PrefService* prefs = content::Source<PrefService>(source).ptr();
  DCHECK(prefs == pref_service_);
  std::string* pref_name = content::Details<std::string>(details).ptr();
  if (*pref_name == prefs::kHttpServerProperties) {
    if (!setting_prefs_)
      ScheduleUpdateCacheOnUI();
  } else {
    NOTREACHED();
  }
}

}  // namespace chrome_browser_net
