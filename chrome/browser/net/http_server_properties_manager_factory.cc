// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_server_properties_manager_factory.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_server_properties_manager.h"

namespace chrome_browser_net {

/* static */
net::HttpServerPropertiesManager*
HttpServerPropertiesManagerFactory::CreateManager(PrefService* pref_service) {
  using content::BrowserThread;
  return new net::HttpServerPropertiesManager(
      pref_service,
      prefs::kHttpServerProperties,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}

/* static */
void HttpServerPropertiesManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kHttpServerProperties,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace chrome_browser_net
