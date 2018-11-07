// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "chrome/browser/apps/foundation/app_service/app_registry/app_registry.h"

#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Key for synced App Registry prefs. The pref format is a dictionary where the
// key is an app ID and the value is a dictionary of app metadata.
constexpr char kAppRegistryPrefNameSynced[] = "app_registry_synced";

// Key for per-app preferences within the app metadata dictionary. Used to store
// arbitrary data per each app.
constexpr char kAppRegistryPrefNamePrefs[] = "prefs";

// Key for whether an app should be preferred for opening links.
constexpr char kAppRegistryPrefKeyPreferred[] = "preferred";

}  // namespace

namespace apps {

// static
void AppRegistry::RegisterPrefs(PrefRegistrySimple* registry) {
  // Note: it may be desirable to add a non-synced pref if some future app
  // metadata should remain local to the device.
  registry->RegisterDictionaryPref(
      kAppRegistryPrefNameSynced,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

AppRegistry::AppRegistry(std::unique_ptr<PrefService> pref_service)
    : pref_service_(std::move(pref_service)) {}

AppRegistry::~AppRegistry() = default;

void AppRegistry::BindRequest(apps::mojom::AppRegistryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppRegistry::GetApps(GetAppsCallback callback) {
  std::vector<apps::mojom::AppInfoPtr> app_infos;
  const base::DictionaryValue* apps =
      pref_service_->GetDictionary(kAppRegistryPrefNameSynced);

  for (const auto& item : apps->DictItems()) {
    auto state = apps::mojom::AppPreferred::kUnknown;
    const base::Value* value = item.second.FindPathOfType(
        {kAppRegistryPrefNamePrefs, kAppRegistryPrefKeyPreferred},
        base::Value::Type::INTEGER);
    if (value)
      state = static_cast<apps::mojom::AppPreferred>(value->GetInt());

    auto app_info = apps::mojom::AppInfo::New();
    app_info->id = item.first;
    app_info->type = apps::mojom::AppType::kUnknown;
    app_info->preferred = state;
    app_infos.push_back(std::move(app_info));
  }

  std::move(callback).Run(std::move(app_infos));
}

apps::mojom::AppPreferred AppRegistry::GetIfAppPreferredForTesting(
    const std::string& app_id) const {
  const base::Value* value =
      pref_service_->GetDictionary(kAppRegistryPrefNameSynced)
          ->FindPathOfType(
              {app_id, kAppRegistryPrefNamePrefs, kAppRegistryPrefKeyPreferred},
              base::Value::Type::INTEGER);
  return value ? static_cast<apps::mojom::AppPreferred>(value->GetInt())
               : apps::mojom::AppPreferred::kUnknown;
}

void AppRegistry::SetAppPreferred(const std::string& app_id,
                                  apps::mojom::AppPreferred state) {
  DictionaryPrefUpdate dict_update(pref_service_.get(),
                                   kAppRegistryPrefNameSynced);
  dict_update->SetPath(
      {app_id, kAppRegistryPrefNamePrefs, kAppRegistryPrefKeyPreferred},
      base::Value(static_cast<int>(state)));
}

}  // namespace apps
