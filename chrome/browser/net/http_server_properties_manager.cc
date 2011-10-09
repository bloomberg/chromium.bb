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
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

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
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui_method_factory_(this)),
      pref_service_(pref_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(pref_service);
  ui_weak_ptr_factory_.reset(
      new base::WeakPtrFactory<HttpServerPropertiesManager>(this));
  ui_weak_ptr_ = ui_weak_ptr_factory_->GetWeakPtr();
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(prefs::kSpdyServers, this);
}

HttpServerPropertiesManager::~HttpServerPropertiesManager() {
}

void HttpServerPropertiesManager::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  http_server_properties_impl_.reset(new net::HttpServerPropertiesImpl());

  io_method_factory_.reset(
      new ScopedRunnableMethodFactory<HttpServerPropertiesManager>(this));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdateCacheFromPrefs,
                 ui_weak_ptr_));
}

void HttpServerPropertiesManager::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel any pending updates, and stop listening for pref change updates.
  ui_method_factory_.RevokeAll();
  ui_weak_ptr_factory_.reset();
  pref_change_registrar_.RemoveAll();
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

void HttpServerPropertiesManager::DeleteAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  http_server_properties_impl_->DeleteAll();
  ScheduleUpdatePrefsOnIO();
}

// static
void HttpServerPropertiesManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kSpdyServers, PrefService::UNSYNCABLE_PREF);
}

//
// Update spdy_servers (the cached data) with data from preferences.
//
void HttpServerPropertiesManager::ScheduleUpdateCacheOnUI() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel pending updates, if any.
  ui_method_factory_.RevokeAll();
  PostUpdateTaskOnUI(
      ui_method_factory_.NewRunnableMethod(
          &HttpServerPropertiesManager::UpdateCacheFromPrefs));
}

void HttpServerPropertiesManager::PostUpdateTaskOnUI(Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This is overridden in tests to post the task without the delay.
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, kUpdateCacheDelayMs);
}

void HttpServerPropertiesManager::UpdateCacheFromPrefs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!pref_service_->HasPrefPath(prefs::kSpdyServers))
    return;

  // The preferences can only be read on the UI thread.
  StringVector* spdy_servers =
      ListValueToStringVector(pref_service_->GetList(prefs::kSpdyServers));

  // Go through the IO thread to grab a WeakPtr to |this|. This is safe from
  // here, since this task will always execute before a potential deletion of
  // ProfileIOData on IO.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServerPropertiesManager::UpdateCacheFromPrefsOnIO,
                 base::Unretained(this), spdy_servers, true));
}

void HttpServerPropertiesManager::UpdateCacheFromPrefsOnIO(
    StringVector* spdy_servers,
    bool support_spdy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Preferences have the master data because admins might have pushed new
  // preferences. Clear the cached data and use the new spdy server list from
  // preferences.
  scoped_ptr<StringVector> scoped_spdy_servers(spdy_servers);
  http_server_properties_impl_->Initialize(spdy_servers, support_spdy);
}

//
// Update Preferences with data from spdy_servers (the cached data).
//
void HttpServerPropertiesManager::ScheduleUpdatePrefsOnIO() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Cancel pending updates, if any.
  io_method_factory_->RevokeAll();
  PostUpdateTaskOnIO(
      io_method_factory_->NewRunnableMethod(
          &HttpServerPropertiesManager::UpdatePrefsFromCache));
}

void HttpServerPropertiesManager::PostUpdateTaskOnIO(Task* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // This is overridden in tests to post the task without the delay.
  MessageLoop::current()->PostDelayedTask(FROM_HERE, task, kUpdatePrefsDelayMs);
}

void HttpServerPropertiesManager::UpdatePrefsFromCache() {
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
  pref_service_->Set(prefs::kSpdyServers, spdy_server_list->data);
}

void HttpServerPropertiesManager::Observe(int type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_PREF_CHANGED);
  PrefService* prefs = Source<PrefService>(source).ptr();
  DCHECK(prefs == pref_service_);
  std::string* pref_name = Details<std::string>(details).ptr();
  DCHECK(*pref_name == prefs::kSpdyServers);
  ScheduleUpdateCacheOnUI();
}

}  // namespace chrome_browser_net
