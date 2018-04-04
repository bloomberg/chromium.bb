// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

using vm_tools::apps::App;

namespace chromeos {

namespace {

constexpr char kCrostiniRegistryPref[] = "crostini.registry";

constexpr char kAppDesktopFileIdKey[] = "desktop_file_id";
constexpr char kAppNameKey[] = "name";

constexpr char kCrostiniAppIdPrefix[] = "crostini:";

std::string GenerateAppId(const std::string& desktop_file_id) {
  return crx_file::id_util::GenerateId(kCrostiniAppIdPrefix + desktop_file_id);
}

}  // namespace

CrostiniRegistryService::Registration::Registration(
    const std::string& desktop_file_id,
    const std::string& name)
    : desktop_file_id(desktop_file_id), name(name) {}

CrostiniRegistryService::CrostiniRegistryService(Profile* profile)
    : prefs_(profile->GetPrefs()) {}

CrostiniRegistryService::~CrostiniRegistryService() = default;

std::unique_ptr<CrostiniRegistryService::Registration>
CrostiniRegistryService::GetRegistration(const std::string& app_id) const {
  DCHECK(crx_file::id_util::IdIsValid(app_id));
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(kCrostiniRegistryPref);
  const base::Value* pref_registration =
      apps->FindKeyOfType(app_id, base::Value::Type::DICTIONARY);
  if (!pref_registration)
    return nullptr;

  const base::Value* desktop_file_id = pref_registration->FindKeyOfType(
      kAppDesktopFileIdKey, base::Value::Type::STRING);
  const base::Value* name =
      pref_registration->FindKeyOfType(kAppNameKey, base::Value::Type::STRING);

  return std::make_unique<Registration>(desktop_file_id->GetString(),
                                        name->GetString());
}

void CrostiniRegistryService::UpdateApplicationList(
    const vm_tools::apps::ApplicationList& app_list) {
  DictionaryPrefUpdate update(prefs_, kCrostiniRegistryPref);
  base::DictionaryValue* apps = update.Get();
  apps->Clear();

  for (const App& app : app_list.apps()) {
    if (app.desktop_file_id().empty()) {
      LOG(WARNING) << "Received app with missing desktop file id";
      continue;
    }

    std::string default_name;
    for (const App::LocaleString::Entry& localized_name : app.name().values()) {
      if (localized_name.locale().empty()) {
        default_name = localized_name.value();
        break;
      }
    }
    if (default_name.empty()) {
      LOG(WARNING) << "Received app '" << app.desktop_file_id()
                   << "' with missing unlocalized name";
      continue;
    }

    base::Value pref_registration(base::Value::Type::DICTIONARY);
    pref_registration.SetKey(kAppDesktopFileIdKey,
                             base::Value(app.desktop_file_id()));
    pref_registration.SetKey(kAppNameKey, base::Value(default_name));
    apps->SetKey(GenerateAppId(app.desktop_file_id()),
                 std::move(pref_registration));
  }
}

// static
void CrostiniRegistryService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kCrostiniRegistryPref);
}

}  // namespace chromeos
