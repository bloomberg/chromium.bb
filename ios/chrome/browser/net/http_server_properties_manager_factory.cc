// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/http_server_properties_manager_factory.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/web/public/web_thread.h"
#include "net/http/http_server_properties_manager.h"

// static
net::HttpServerPropertiesManager*
HttpServerPropertiesManagerFactory::CreateManager(PrefService* pref_service) {
  return new net::HttpServerPropertiesManager(
      pref_service, ios::prefs::kHttpServerProperties,
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));
}

// static
void HttpServerPropertiesManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(ios::prefs::kHttpServerProperties);
}
