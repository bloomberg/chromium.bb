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

// Time to wait before starting an update the spdy_servers_table_ cache from
// preferences. Scheduling another update during this period will reset the
// timer.
const int64 kUpdateCacheDelayMs = 1000;

// Time to wait before starting an update the preferences from the
// spdy_servers cache. Scheduling another update during this period will
// reset the timer.
const int64 kUpdatePrefsDelayMs = 5000;

typedef std::vector<std::string> StringVector;

// String is host/port pair of spdy server.
StringVector* ListValueToStringVector(const base::ListValue* list) {
  StringVector* vector = new StringVector;

  if (!list)
    return vector;

  vector->reserve(list->GetSize());
  std::string s;
  for (base::ListValue::const_iterator it = list->begin();
       it != list->end(); ++it) {
    if ((*it)->GetAsString(&s))
      vector->push_back(s);
  }

  return vector;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  HttpServerPropertiesManager

HttpServerPropertiesManager::HttpServerPropertiesManager(
    PrefService* pref_service)
    : pref_service_(pref_service),
      setting_spdy_servers_(false),
      setting_alternate_protocol_servers_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(pref_service);
  ui_weak_ptr_factory_.reset(
      new base::WeakPtrFactory<HttpServerPropertiesManager>(this));
  ui_weak_ptr_ = ui_weak_ptr_factory_->GetWeakPtr();
  ui_spdy_cache_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);
  ui_alternate_protocol_cache_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(prefs::kSpdyServers, this);
  pref_change_registrar_.Add(prefs::kAlternateProtocolServers, this);
}

HttpServerPropertiesManager::~HttpServerPropertiesManager() {
}

void HttpServerPropertiesManager::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_.reset(new net::HttpServerPropertiesImpl());

  io_spdy_prefs_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);
  io_alternate_protocol_prefs_update_timer_.reset(
      new base::OneShotTimer<HttpServerPropertiesManager>);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdateSpdyCacheFromPrefs,
                 ui_weak_ptr_));
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &HttpServerPropertiesManager::UpdateAlternateProtocolCacheFromPrefs,
          ui_weak_ptr_));
}

void HttpServerPropertiesManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel any pending updates, and stop listening for pref change updates.
  ui_spdy_cache_update_timer_->Stop();
  ui_alternate_protocol_cache_update_timer_->Stop();
  ui_weak_ptr_factory_.reset();
  pref_change_registrar_.RemoveAll();
}

// static
void HttpServerPropertiesManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kSpdyServers, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kAlternateProtocolServers,
                                PrefService::UNSYNCABLE_PREF);
}

void HttpServerPropertiesManager::Clear() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->Clear();
  ScheduleUpdateSpdyPrefsOnIO();
  ScheduleUpdateAlternateProtocolPrefsOnIO();
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
  ScheduleUpdateSpdyPrefsOnIO();
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
  ScheduleUpdateAlternateProtocolPrefsOnIO();
}

void HttpServerPropertiesManager::SetBrokenAlternateProtocol(
    const net::HostPortPair& server) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_->SetBrokenAlternateProtocol(server);
  ScheduleUpdateAlternateProtocolPrefsOnIO();
}

const net::AlternateProtocolMap&
HttpServerPropertiesManager::alternate_protocol_map() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return http_server_properties_impl_->alternate_protocol_map();
}

//
// Update spdy_servers (the cached data) with data from preferences.
//
void HttpServerPropertiesManager::ScheduleUpdateSpdyCacheOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel pending updates, if any.
  ui_spdy_cache_update_timer_->Stop();
  StartSpdyCacheUpdateTimerOnUI(
      base::TimeDelta::FromMilliseconds(kUpdateCacheDelayMs));
}

void HttpServerPropertiesManager::UpdateSpdyCacheFromPrefs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!pref_service_->HasPrefPath(prefs::kSpdyServers))
    return;

  // The preferences can only be read on the UI thread.
  StringVector* spdy_servers =
      ListValueToStringVector(pref_service_->GetList(prefs::kSpdyServers));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdateSpdyCacheFromPrefsOnIO,
                 base::Unretained(this), spdy_servers, true));
}

void HttpServerPropertiesManager::UpdateSpdyCacheFromPrefsOnIO(
    StringVector* spdy_servers,
    bool support_spdy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Preferences have the master data because admins might have pushed new
  // preferences. Clear the cached data and use the new spdy server list from
  // preferences.
  scoped_ptr<StringVector> scoped_spdy_servers(spdy_servers);
  http_server_properties_impl_->InitializeSpdyServers(spdy_servers,
                                                      support_spdy);
}

//
// Update Preferences with data from spdy_servers (the cached data).
//
void HttpServerPropertiesManager::ScheduleUpdateSpdyPrefsOnIO() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Cancel pending updates, if any.
  io_spdy_prefs_update_timer_->Stop();
  StartSpdyPrefsUpdateTimerOnIO(
      base::TimeDelta::FromMilliseconds(kUpdatePrefsDelayMs));
}

void HttpServerPropertiesManager::UpdateSpdyPrefsFromCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<RefCountedListValue> spdy_server_list =
      new RefCountedListValue();
  http_server_properties_impl_->GetSpdyServerList(&spdy_server_list->data);

  // Update the preferences on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::SetSpdyServersInPrefsOnUI,
                 ui_weak_ptr_, spdy_server_list));
}

void HttpServerPropertiesManager::SetSpdyServersInPrefsOnUI(
    scoped_refptr<RefCountedListValue> spdy_server_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  setting_spdy_servers_ = true;
  pref_service_->Set(prefs::kSpdyServers, spdy_server_list->data);
  setting_spdy_servers_ = false;
}

void HttpServerPropertiesManager::ScheduleUpdateAlternateProtocolCacheOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel pending updates, if any.
  ui_alternate_protocol_cache_update_timer_->Stop();
  StartAlternateProtocolCacheUpdateTimerOnUI(
      base::TimeDelta::FromMilliseconds(kUpdateCacheDelayMs));
}

void HttpServerPropertiesManager::UpdateAlternateProtocolCacheFromPrefs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!pref_service_->HasPrefPath(prefs::kAlternateProtocolServers))
    return;

  // Parse the preferences into a AlternateProtocolMap.
  net::AlternateProtocolMap alternate_protocol_map;
  const base::DictionaryValue& alternate_protocol_servers =
      *pref_service_->GetDictionary(prefs::kAlternateProtocolServers);
  for (base::DictionaryValue::key_iterator it =
       alternate_protocol_servers.begin_keys();
       it != alternate_protocol_servers.end_keys(); ++it) {
    const std::string& server_str = *it;
    net::HostPortPair server = net::HostPortPair::FromString(server_str);
    if (server.host().empty()) {
      VLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
      NOTREACHED();
      continue;
    }
    DCHECK(!ContainsKey(alternate_protocol_map, server));

    base::DictionaryValue* port_alternate_protocol_dict = NULL;
    if (!alternate_protocol_servers.GetDictionaryWithoutPathExpansion(
        server_str, &port_alternate_protocol_dict)) {
      VLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
      NOTREACHED();
      continue;
    }

    int port = 0;
    if (!port_alternate_protocol_dict->GetIntegerWithoutPathExpansion(
        "port", &port) || (port > (1 << 16))) {
      VLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
      NOTREACHED();
      continue;
    }
    int protocol = 0;
    if (!port_alternate_protocol_dict->GetIntegerWithoutPathExpansion(
        "protocol", &protocol) || (protocol < 0) ||
        (protocol > net::NUM_ALTERNATE_PROTOCOLS)) {
      VLOG(1) << "Malformed Alternate-Protocol server: " << server_str;
      NOTREACHED();
      continue;
    }

    net::PortAlternateProtocolPair port_alternate_protocol;
    port_alternate_protocol.port = port;
    port_alternate_protocol.protocol = static_cast<net::AlternateProtocol>(
        protocol);

    alternate_protocol_map[server] = port_alternate_protocol;
  }

  scoped_refptr<RefCountedAlternateProtocolMap> alternate_protocol_map_arg =
      new RefCountedAlternateProtocolMap;
  alternate_protocol_map_arg->data.swap(alternate_protocol_map);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::
                 UpdateAlternateProtocolCacheFromPrefsOnIO,
                 base::Unretained(this), alternate_protocol_map_arg));
}

void HttpServerPropertiesManager::UpdateAlternateProtocolCacheFromPrefsOnIO(
      RefCountedAlternateProtocolMap* alternate_protocol_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Preferences have the master data because admins might have pushed new
  // preferences. Clear the cached data and use the new spdy server list from
  // preferences.
  http_server_properties_impl_->InitializeAlternateProtocolServers(
      &alternate_protocol_map->data);
}

//
// Update Preferences with data from alternate_protocol_servers (the cached
// data).
//
void HttpServerPropertiesManager::ScheduleUpdateAlternateProtocolPrefsOnIO() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Cancel pending updates, if any.
  io_alternate_protocol_prefs_update_timer_->Stop();
  StartAlternateProtocolPrefsUpdateTimerOnIO(
      base::TimeDelta::FromMilliseconds(kUpdatePrefsDelayMs));
}

void HttpServerPropertiesManager::UpdateAlternateProtocolPrefsFromCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  scoped_refptr<RefCountedAlternateProtocolMap> alternate_protocol_map =
      new RefCountedAlternateProtocolMap;
  alternate_protocol_map->data =
      http_server_properties_impl_->alternate_protocol_map();

  // Update the preferences on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::
                 SetAlternateProtocolServersInPrefsOnUI,
                 ui_weak_ptr_, alternate_protocol_map));
}

void HttpServerPropertiesManager::SetAlternateProtocolServersInPrefsOnUI(
    RefCountedAlternateProtocolMap* alternate_protocol_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::DictionaryValue alternate_protocol_dict;
  for (net::AlternateProtocolMap::const_iterator it =
       alternate_protocol_map->data.begin();
       it != alternate_protocol_map->data.end(); ++it) {
    const net::HostPortPair& server = it->first;
    const net::PortAlternateProtocolPair& port_alternate_protocol =
        it->second;
    if (port_alternate_protocol.protocol < 0 ||
        port_alternate_protocol.protocol >= net::NUM_ALTERNATE_PROTOCOLS) {
      continue;
    }
    base::DictionaryValue* port_alternate_protocol_dict =
        new base::DictionaryValue;
    port_alternate_protocol_dict->SetInteger(
        "port", port_alternate_protocol.port);
    port_alternate_protocol_dict->SetInteger(
        "protocol", port_alternate_protocol.protocol);
    alternate_protocol_dict.SetWithoutPathExpansion(
        server.ToString(), port_alternate_protocol_dict);
  }
  setting_alternate_protocol_servers_ = true;
  pref_service_->Set(prefs::kAlternateProtocolServers,
                     alternate_protocol_dict);
  setting_alternate_protocol_servers_ = false;
}

void HttpServerPropertiesManager::StartSpdyCacheUpdateTimerOnUI(
    base::TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui_spdy_cache_update_timer_->Start(
      FROM_HERE, delay, this,
      &HttpServerPropertiesManager::UpdateSpdyCacheFromPrefs);
}

void HttpServerPropertiesManager::StartSpdyPrefsUpdateTimerOnIO(
    base::TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // This is overridden in tests to post the task without the delay.
  io_spdy_prefs_update_timer_->Start(
      FROM_HERE, delay, this,
      &HttpServerPropertiesManager::UpdateSpdyPrefsFromCache);
}

void HttpServerPropertiesManager::StartAlternateProtocolCacheUpdateTimerOnUI(
    base::TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ui_alternate_protocol_cache_update_timer_->Start(
      FROM_HERE, delay, this,
      &HttpServerPropertiesManager::UpdateAlternateProtocolCacheFromPrefs);
}

void HttpServerPropertiesManager::StartAlternateProtocolPrefsUpdateTimerOnIO(
    base::TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // This is overridden in tests to post the task without the delay.
  io_alternate_protocol_prefs_update_timer_->Start(
      FROM_HERE, delay, this,
      &HttpServerPropertiesManager::UpdateAlternateProtocolPrefsFromCache);
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
  if (*pref_name == prefs::kSpdyServers) {
    if (!setting_spdy_servers_)
      ScheduleUpdateSpdyCacheOnUI();
  } else if (*pref_name == prefs::kAlternateProtocolServers) {
    if (!setting_alternate_protocol_servers_)
      ScheduleUpdateAlternateProtocolCacheOnUI();
  } else {
    NOTREACHED();
  }
}

}  // namespace chrome_browser_net
