// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace web_app {

// static
void ExtensionIdsMap::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kWebAppsExtensionIDs);
}

// static
bool ExtensionIdsMap::HasExtensionId(const PrefService* pref_service,
                                     const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::DictionaryValue* dict =
      pref_service->GetDictionary(prefs::kWebAppsExtensionIDs);
  if (!dict) {
    return false;
  }
  // Do a simple O(N) scan for extension_id being a value in the dictionary's
  // key/value pairs. We expect both N and the number of times HasExtensionId
  // is called to be relatively small in practice. If they turn out to be
  // large, we can write a more sophisticated implementation.
  for (const auto& it : dict->DictItems()) {
    if (it.second.is_string() && it.second.GetString() == extension_id) {
      return true;
    }
  }
  return false;
}

ExtensionIdsMap::ExtensionIdsMap(PrefService* pref_service)
    : pref_service_(pref_service) {}

void ExtensionIdsMap::Insert(const GURL& url, const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DictionaryPrefUpdate dict_update(pref_service_, prefs::kWebAppsExtensionIDs);
  dict_update->SetKey(url.spec(), base::Value(extension_id));
}

base::Optional<std::string> ExtensionIdsMap::Lookup(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* value =
      pref_service_->GetDictionary(prefs::kWebAppsExtensionIDs)
          ->FindKeyOfType(url.spec(), base::Value::Type::STRING);
  return value ? base::make_optional(value->GetString()) : base::nullopt;
}

}  // namespace web_app
