// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

using vm_tools::apps::App;

namespace chromeos {

namespace {

constexpr char kCrostiniRegistryPref[] = "crostini.registry";

// Keys for the Dictionary stored in prefs for each app.
constexpr char kAppDesktopFileIdKey[] = "desktop_file_id";
constexpr char kAppVmNameKey[] = "vm_name";
constexpr char kAppContainerNameKey[] = "container_name";
constexpr char kAppCommentKey[] = "comment";
constexpr char kAppMimeTypesKey[] = "mime_types";
constexpr char kAppNameKey[] = "name";
constexpr char kAppNoDisplayKey[] = "no_display";

constexpr char kCrostiniAppIdPrefix[] = "crostini:";

std::string GenerateAppId(const std::string& desktop_file_id,
                          const std::string& vm_name,
                          const std::string& container_name) {
  // These can collide in theory because the user could choose VM and container
  // names which contain slashes, but this will only result in apps missing from
  // the launcher.
  return crx_file::id_util::GenerateId(kCrostiniAppIdPrefix + vm_name + "/" +
                                       container_name + "/" + desktop_file_id);
}

std::map<std::string, std::string> DictionaryToStringMap(
    const base::Value* value) {
  std::map<std::string, std::string> result;
  for (const auto& item : value->DictItems())
    result[item.first] = item.second.GetString();
  return result;
}

base::Value ProtoToDictionary(const App::LocaleString& locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const App::LocaleString::Entry& entry : locale_string.values()) {
    const std::string& locale = entry.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;

    result.SetKey(locale, base::Value(entry.value()));
  }
  return result;
}

std::vector<std::string> ListToStringVector(const base::Value* list) {
  std::vector<std::string> result;
  for (const base::Value& value : list->GetList())
    result.emplace_back(value.GetString());
  return result;
}

base::Value ProtoToList(
    const google::protobuf::RepeatedPtrField<std::string>& strings) {
  base::Value result(base::Value::Type::LIST);
  for (const std::string& string : strings)
    result.GetList().emplace_back(string);
  return result;
}

}  // namespace

CrostiniRegistryService::Registration::Registration(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name,
    const LocaleString& name,
    const LocaleString& comment,
    const std::vector<std::string>& mime_types,
    bool no_display)
    : desktop_file_id(desktop_file_id),
      vm_name(vm_name),
      container_name(container_name),
      name(name),
      comment(comment),
      mime_types(mime_types),
      no_display(no_display) {
  DCHECK(name.find(std::string()) != name.end());
}

CrostiniRegistryService::Registration::~Registration() = default;

// static
const std::string& CrostiniRegistryService::Registration::Localize(
    const LocaleString& locale_string) {
  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);

  for (const std::string& locale : locales) {
    LocaleString::const_iterator it = locale_string.find(locale);
    if (it != locale_string.end())
      return it->second;
  }
  return locale_string.at(std::string());
}

CrostiniRegistryService::CrostiniRegistryService(Profile* profile)
    : prefs_(profile->GetPrefs()) {}

CrostiniRegistryService::~CrostiniRegistryService() = default;

std::vector<std::string> CrostiniRegistryService::GetRegisteredAppIds() const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(kCrostiniRegistryPref);
  std::vector<std::string> result;
  for (const auto& item : apps->DictItems())
    result.push_back(item.first);

  return result;
}

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
  const base::Value* vm_name = pref_registration->FindKeyOfType(
      kAppVmNameKey, base::Value::Type::STRING);
  const base::Value* container_name = pref_registration->FindKeyOfType(
      kAppContainerNameKey, base::Value::Type::STRING);

  const base::Value* name = pref_registration->FindKeyOfType(
      kAppNameKey, base::Value::Type::DICTIONARY);
  const base::Value* comment = pref_registration->FindKeyOfType(
      kAppCommentKey, base::Value::Type::DICTIONARY);
  const base::Value* mime_types = pref_registration->FindKeyOfType(
      kAppMimeTypesKey, base::Value::Type::LIST);
  const base::Value* no_display = pref_registration->FindKeyOfType(
      kAppNoDisplayKey, base::Value::Type::BOOLEAN);

  return std::make_unique<Registration>(
      desktop_file_id->GetString(), vm_name->GetString(),
      container_name->GetString(), DictionaryToStringMap(name),
      DictionaryToStringMap(comment), ListToStringVector(mime_types),
      no_display->GetBool());
}

void CrostiniRegistryService::UpdateApplicationList(
    const vm_tools::apps::ApplicationList& app_list) {
  if (app_list.vm_name().empty()) {
    LOG(WARNING) << "Received app list with missing VM name";
    return;
  }
  if (app_list.container_name().empty()) {
    LOG(WARNING) << "Received app list with missing container name";
    return;
  }

  // We need to compute the diff between the new list of apps and the old list
  // of apps (with matching vm/container names). We keep a set of the new app
  // ids so that we can compute these and update the Dictionary directly.
  std::set<std::string> new_app_ids;
  std::vector<std::string> updated_apps;
  std::vector<std::string> removed_apps;
  std::vector<std::string> inserted_apps;

  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, kCrostiniRegistryPref);
    base::DictionaryValue* apps = update.Get();
    for (const App& app : app_list.apps()) {
      if (app.desktop_file_id().empty()) {
        LOG(WARNING) << "Received app with missing desktop file id";
        continue;
      }

      base::Value name = ProtoToDictionary(app.name());
      if (name.FindKey(base::StringPiece()) == nullptr) {
        LOG(WARNING) << "Received app '" << app.desktop_file_id()
                     << "' with missing unlocalized name";
        continue;
      }

      std::string app_id = GenerateAppId(
          app.desktop_file_id(), app_list.vm_name(), app_list.container_name());
      new_app_ids.insert(app_id);

      base::Value pref_registration(base::Value::Type::DICTIONARY);
      pref_registration.SetKey(kAppDesktopFileIdKey,
                               base::Value(app.desktop_file_id()));
      pref_registration.SetKey(kAppVmNameKey, base::Value(app_list.vm_name()));
      pref_registration.SetKey(kAppContainerNameKey,
                               base::Value(app_list.container_name()));
      pref_registration.SetKey(kAppNameKey, std::move(name));
      pref_registration.SetKey(kAppCommentKey,
                               ProtoToDictionary(app.comment()));
      pref_registration.SetKey(kAppMimeTypesKey, ProtoToList(app.mime_types()));
      pref_registration.SetKey(kAppNoDisplayKey, base::Value(app.no_display()));

      base::Value* old_app = apps->FindKey(app_id);
      if (old_app && pref_registration == *old_app)
        continue;

      if (old_app)
        updated_apps.push_back(app_id);
      else
        inserted_apps.push_back(app_id);

      apps->SetKey(app_id, std::move(pref_registration));
    }

    for (const auto& item : apps->DictItems()) {
      if (item.second.FindKey(kAppVmNameKey)->GetString() ==
              app_list.vm_name() &&
          item.second.FindKey(kAppContainerNameKey)->GetString() ==
              app_list.container_name() &&
          new_app_ids.find(item.first) == new_app_ids.end()) {
        removed_apps.push_back(item.first);
      }
    }

    for (const std::string& removed_app : removed_apps)
      apps->RemoveKey(removed_app);
  }

  for (Observer& obs : observers_)
    obs.OnRegistryUpdated(this, updated_apps, removed_apps, inserted_apps);
}

void CrostiniRegistryService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CrostiniRegistryService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void CrostiniRegistryService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kCrostiniRegistryPref);
}

}  // namespace chromeos
