// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

using vm_tools::apps::App;

namespace crostini {

namespace {

// Prefixes of the ApplicationId set on exo windows.
constexpr char kArcWindowAppIdPrefix[] = "org.chromium.arc";
constexpr char kCrostiniWindowAppIdPrefix[] = "org.chromium.termina.";
// This comes after kCrostiniWindowAppIdPrefix
constexpr char kWMClassPrefix[] = "wmclass.";

// This prefix is used when generating the crostini app list id, and used as a
// prefix when generating shelf ids for windows we couldn't match to an app.
constexpr char kCrostiniAppIdPrefix[] = "crostini:";

constexpr char kCrostiniRegistryPref[] = "crostini.registry";

// Keys for the Dictionary stored in prefs for each app.
constexpr char kAppDesktopFileIdKey[] = "desktop_file_id";
constexpr char kAppVmNameKey[] = "vm_name";
constexpr char kAppContainerNameKey[] = "container_name";
constexpr char kAppCommentKey[] = "comment";
constexpr char kAppMimeTypesKey[] = "mime_types";
constexpr char kAppNameKey[] = "name";
constexpr char kAppNoDisplayKey[] = "no_display";
constexpr char kAppStartupWMClassKey[] = "startup_wm_class";
constexpr char kAppStartupNotifyKey[] = "startup_notify";

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

enum class FindAppIdResult { NoMatch, UniqueMatch, NonUniqueMatch };
// Looks for an app where prefs_key is set to search_value. Returns the apps id
// if there was only one app matching, otherwise returns an empty string.
FindAppIdResult FindAppId(const base::DictionaryValue* prefs,
                          base::StringPiece prefs_key,
                          base::StringPiece search_value,
                          std::string* result,
                          bool require_startup_notify = false) {
  result->clear();
  for (const auto& item : prefs->DictItems()) {
    if (require_startup_notify &&
        !item.second
             .FindKeyOfType(kAppStartupNotifyKey, base::Value::Type::BOOLEAN)
             ->GetBool())
      continue;

    const std::string& value =
        item.second.FindKeyOfType(prefs_key, base::Value::Type::STRING)
            ->GetString();
    if (!EqualsCaseInsensitiveASCII(search_value, value))
      continue;

    if (!result->empty())
      return FindAppIdResult::NonUniqueMatch;
    *result = item.first;
  }

  if (!result->empty())
    return FindAppIdResult::UniqueMatch;
  return FindAppIdResult::NoMatch;
}

}  // namespace

CrostiniRegistryService::Registration::Registration(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name,
    const LocaleString& name,
    const LocaleString& comment,
    const std::vector<std::string>& mime_types,
    bool no_display,
    const std::string& startup_wm_class,
    bool startup_notify)
    : desktop_file_id(desktop_file_id),
      vm_name(vm_name),
      container_name(container_name),
      name(name),
      comment(comment),
      mime_types(mime_types),
      no_display(no_display),
      startup_wm_class(startup_wm_class),
      startup_notify(startup_notify) {
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

// The code follows these steps to identify apps and returns the first match:
// 1) Ignore windows if the App Id is prefixed by org.chromium.arc.
// 2) If the Startup Id is set, look for a matching desktop file id.
// 3) If the App Id is not prefixed by org.chromium.termina., it's an app with
// native Wayland support. Look for a matching desktop file id.
// 4) If the App Id is prefixed by org.chromium.wmclass.:
// 4.1) Look for an app where StartupWMClass is matches the suffix.
// 4.2) Look for an app where the desktop file id matches the suffix.
// 5) If we couldn't find a match, prefix the app id with 'crostini:' so we can
// easily identify shelf entries as Crostini apps.
std::string CrostiniRegistryService::GetCrostiniShelfAppId(
    const std::string& window_app_id,
    const std::string* window_startup_id) {
  if (base::StartsWith(window_app_id, kArcWindowAppIdPrefix,
                       base::CompareCase::SENSITIVE))
    return std::string();

  const base::DictionaryValue* apps =
      prefs_->GetDictionary(kCrostiniRegistryPref);
  std::string app_id;

  if (window_startup_id) {
    // TODO(timloh): We should use a value that is unique so we can handle
    // an app installed in multiple containers.
    if (FindAppId(apps, kAppDesktopFileIdKey, *window_startup_id, &app_id,
                  true) == FindAppIdResult::UniqueMatch)
      return app_id;
    LOG(ERROR) << "Startup ID was set to '" << *window_startup_id
               << "' but not matched";
    // Try a lookup with the window app id.
  }

  // Wayland apps won't be prefixed with org.chromium.termina.
  if (!base::StartsWith(window_app_id, kCrostiniWindowAppIdPrefix,
                        base::CompareCase::SENSITIVE)) {
    if (FindAppId(apps, kAppDesktopFileIdKey, window_app_id, &app_id) ==
        FindAppIdResult::UniqueMatch)
      return app_id;
    return kCrostiniAppIdPrefix + window_app_id;
  }

  base::StringPiece suffix(
      window_app_id.begin() + strlen(kCrostiniWindowAppIdPrefix),
      window_app_id.end());

  // If we don't have an id to match to a desktop file, use the window app id.
  if (!base::StartsWith(suffix, kWMClassPrefix, base::CompareCase::SENSITIVE))
    return kCrostiniAppIdPrefix + window_app_id;

  // If an app had StartupWMClass set to the given WM class, use that,
  // otherwise look for a desktop file id matching the WM class.
  base::StringPiece key = suffix.substr(strlen(kWMClassPrefix));
  FindAppIdResult result = FindAppId(apps, kAppStartupWMClassKey, key, &app_id);
  if (result == FindAppIdResult::UniqueMatch)
    return app_id;
  if (result == FindAppIdResult::NonUniqueMatch)
    return kCrostiniAppIdPrefix + window_app_id;

  if (FindAppId(apps, kAppDesktopFileIdKey, key, &app_id) ==
      FindAppIdResult::UniqueMatch)
    return app_id;
  return kCrostiniAppIdPrefix + window_app_id;
}

bool CrostiniRegistryService::IsCrostiniShelfAppId(
    const std::string& shelf_app_id) {
  if (base::StartsWith(shelf_app_id, kCrostiniAppIdPrefix,
                       base::CompareCase::SENSITIVE))
    return true;
  if (shelf_app_id == kCrostiniTerminalId)
    return true;
  // TODO(timloh): We need to handle desktop files that have been removed.
  // For example, running windows with a no-longer-valid app id will try to
  // use the ExtensionContextMenuModel.
  return prefs_->GetDictionary(kCrostiniRegistryPref)->FindKey(shelf_app_id) !=
         nullptr;
}

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
  const base::Value* startup_wm_class = pref_registration->FindKeyOfType(
      kAppStartupWMClassKey, base::Value::Type::STRING);
  const base::Value* startup_notify = pref_registration->FindKeyOfType(
      kAppStartupNotifyKey, base::Value::Type::BOOLEAN);

  return std::make_unique<Registration>(
      desktop_file_id->GetString(), vm_name->GetString(),
      container_name->GetString(), DictionaryToStringMap(name),
      DictionaryToStringMap(comment), ListToStringVector(mime_types),
      no_display->GetBool(),
      startup_wm_class ? startup_wm_class->GetString() : "",
      startup_notify && startup_notify->GetBool());
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
      pref_registration.SetKey(kAppStartupWMClassKey,
                               base::Value(app.startup_wm_class()));
      pref_registration.SetKey(kAppStartupNotifyKey,
                               base::Value(app.startup_notify()));

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

}  // namespace crostini
