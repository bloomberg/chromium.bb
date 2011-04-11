// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"

using base::Time;

namespace {

// Additional preferences keys

// Where an extension was installed from. (see Extension::Location)
const char kPrefLocation[] = "location";

// Enabled, disabled, killed, etc. (see Extension::State)
const char kPrefState[] = "state";

// The path to the current version's manifest file.
const char kPrefPath[] = "path";

// The dictionary containing the extension's manifest.
const char kPrefManifest[] = "manifest";

// The version number.
const char kPrefVersion[] = "manifest.version";

// Indicates if an extension is blacklisted:
const char kPrefBlacklist[] = "blacklist";

// Indicates whether to show an install warning when the user enables.
const char kExtensionDidEscalatePermissions[] = "install_warning_on_enable";

// A preference that tracks browser action toolbar configuration. This is a list
// object stored in the Preferences file. The extensions are stored by ID.
const char kExtensionToolbar[] = "extensions.toolbar";

// The key for a serialized Time value indicating the start of the day (from the
// server's perspective) an extension last included a "ping" parameter during
// its update check.
const char kLastPingDay[] = "lastpingday";

// Similar to kLastPingDay, but for "active" instead of "rollcall" pings.
const char kLastActivePingDay[] = "last_active_pingday";

// A bit we use to keep track of whether we need to do an "active" ping.
const char kActiveBit[] = "active_bit";

// Path for settings specific to blacklist update.
const char kExtensionsBlacklistUpdate[] = "extensions.blacklistupdate";

// Path and sub-keys for the idle install info dictionary preference.
const char kIdleInstallInfo[] = "idle_install_info";
const char kIdleInstallInfoCrxPath[] = "crx_path";
const char kIdleInstallInfoVersion[] = "version";
const char kIdleInstallInfoFetchTime[] = "fetch_time";


// A preference that, if true, will allow this extension to run in incognito
// mode.
const char kPrefIncognitoEnabled[] = "incognito";

// A preference to control whether an extension is allowed to inject script in
// pages with file URLs.
const char kPrefAllowFileAccess[] = "allowFileAccess";

// A preference set by the web store to indicate login information for
// purchased apps.
const char kWebStoreLogin[] = "extensions.webstore_login";

// A preference set by the the NTP to persist the desired launch container type
// used for apps.
const char kPrefLaunchType[] = "launchType";

// A preference determining the order of which the apps appear on the NTP.
const char kPrefAppLaunchIndex[] = "app_launcher_index";

// A preference determining the page on which an app appears in the NTP.
const char kPrefPageIndex[] = "page_index";

// A preference specifying if the user dragged the app on the NTP.
const char kPrefUserDraggedApp[] = "user_dragged_app_ntp";

// A preference for storing extra data sent in update checks for an extension.
const char kUpdateUrlData[] = "update_url_data";

// Whether the browser action is visible in the toolbar.
const char kBrowserActionVisible[] = "browser_action_visible";

// Preferences that hold which permissions the user has granted the extension.
// We explicitly keep track of these so that extensions can contain unknown
// permissions, for backwards compatibility reasons, and we can still prompt
// the user to accept them once recognized.
const char kPrefGrantedPermissionsAPI[] = "granted_permissions.api";
const char kPrefGrantedPermissionsHost[] = "granted_permissions.host";
const char kPrefGrantedPermissionsAll[] = "granted_permissions.full";

// A preference that indicates when an extension was installed.
const char kPrefInstallTime[] = "install_time";

// A preference that contains any extension-controlled preferences.
const char kPrefPreferences[] = "preferences";

// Provider of write access to a dictionary storing extension prefs.
class ScopedExtensionPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedExtensionPrefUpdate(PrefService* service,
                            const std::string& extension_id) :
    DictionaryPrefUpdate(service, ExtensionPrefs::kExtensionsPref),
    extension_id_(extension_id) {}
  virtual ~ScopedExtensionPrefUpdate() {}
  virtual DictionaryValue* Get() {
    DictionaryValue* dict = DictionaryPrefUpdate::Get();
    DictionaryValue* extension = NULL;
    if (!dict->GetDictionary(extension_id_, &extension)) {
      // Extension pref does not exist, create it.
      extension = new DictionaryValue();
      dict->Set(extension_id_, extension);
    }
    return extension;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionPrefUpdate);
};

// Provider of write access to a dictionary storing extension controlled prefs.
class ScopedExtensionControlledPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedExtensionControlledPrefUpdate(PrefService* service,
                                      const std::string& extension_id) :
    DictionaryPrefUpdate(service, ExtensionPrefs::kExtensionsPref),
    extension_id_(extension_id) {}
  virtual ~ScopedExtensionControlledPrefUpdate() {}
  virtual DictionaryValue* Get() {
    DictionaryValue* dict = DictionaryPrefUpdate::Get();
    DictionaryValue* preferences = NULL;
    std::string key = extension_id_ + std::string(".") + kPrefPreferences;
    if (!dict->GetDictionary(key, &preferences)) {
      preferences = new DictionaryValue;
      dict->Set(key, preferences);
    }
    return preferences;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionControlledPrefUpdate);
};

// TODO(mihaip): This is cleanup code for keys for unpacked extensions (which
// are derived from paths). As part of the wstring removal, we changed the way
// we hash paths, so we need to move prefs from their old synthesized IDs to
// their new ones. We can remove this by July 2011. (See http://crbug.com/75945
// for more details).
static void CleanupBadExtensionKeys(const FilePath& root_dir,
                                    PrefService* prefs) {
  const DictionaryValue* dictionary =
      prefs->GetDictionary(ExtensionPrefs::kExtensionsPref);
  std::map<std::string, std::string> remapped_keys;
  for (DictionaryValue::key_iterator i = dictionary->begin_keys();
       i != dictionary->end_keys(); ++i) {
    DictionaryValue* ext;
    if (!dictionary->GetDictionaryWithoutPathExpansion(*i, &ext))
      continue;

    int location;
    FilePath::StringType path_str;
    if (!ext->GetInteger(kPrefLocation, &location) ||
        !ext->GetString(kPrefPath, &path_str)) {
      continue;
    }

    // Only unpacked extensions have generated IDs.
    if (location != Extension::LOAD)
      continue;

    const std::string& prefs_id(*i);
    FilePath path(path_str);
    // The persisted path can be relative to the root dir (see
    // MakePath(s)Relative), but the ID is generated before that, using the
    // absolute path, so we need to undo that.
    if (!path.IsAbsolute()) {
      path = root_dir.Append(path);
    }
    std::string computed_id = Extension::GenerateIdForPath(path);

    if (prefs_id != computed_id) {
      remapped_keys[prefs_id] = computed_id;
    }
  }

  if (!remapped_keys.empty()) {
    DictionaryPrefUpdate update(prefs, ExtensionPrefs::kExtensionsPref);
    DictionaryValue* update_dictionary = update.Get();
    for (std::map<std::string, std::string>::const_iterator i =
            remapped_keys.begin();
        i != remapped_keys.end();
        ++i) {
      // Don't clobber prefs under the correct ID if they already exist.
      if (update_dictionary->HasKey(i->second)) {
        CHECK(update_dictionary->RemoveWithoutPathExpansion(i->first, NULL));
        continue;
      }
      Value* extension_prefs = NULL;
      CHECK(update_dictionary->RemoveWithoutPathExpansion(
          i->first, &extension_prefs));
      update_dictionary->SetWithoutPathExpansion(i->second, extension_prefs);
    }

    prefs->ScheduleSavePersistentPrefs();
  }
}

static void ExtentToStringSet(const ExtensionExtent& host_extent,
                              std::set<std::string>* result) {
  ExtensionExtent::PatternList patterns = host_extent.patterns();
  ExtensionExtent::PatternList::const_iterator i;

  for (i = patterns.begin(); i != patterns.end(); ++i)
    result->insert(i->GetAsString());
}

}  // namespace

ExtensionPrefs::ExtensionPrefs(
    PrefService* prefs,
    const FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map)
    : prefs_(prefs),
      install_directory_(root_dir),
      extension_pref_value_map_(extension_pref_value_map) {
  // TODO(mihaip): Remove this by July 2011 (see comment above).
  CleanupBadExtensionKeys(root_dir, prefs_);

  MakePathsRelative();

  InitPrefStore();
}

ExtensionPrefs::~ExtensionPrefs() {}

// static
const char ExtensionPrefs::kExtensionsPref[] = "extensions.settings";

static FilePath::StringType MakePathRelative(const FilePath& parent,
                                             const FilePath& child) {
  if (!parent.IsParent(child))
    return child.value();

  FilePath::StringType retval = child.value().substr(
      parent.value().length());
  if (FilePath::IsSeparator(retval[0]))
    return retval.substr(1);
  else
    return retval;
}

void ExtensionPrefs::MakePathsRelative() {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict || dict->empty())
    return;

  // Collect all extensions ids with absolute paths in |absolute_keys|.
  std::set<std::string> absolute_keys;
  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    DictionaryValue* extension_dict = NULL;
    if (!dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict))
      continue;
    int location_value;
    if (extension_dict->GetInteger(kPrefLocation, &location_value) &&
        location_value == Extension::LOAD) {
      // Unpacked extensions can have absolute paths.
      continue;
    }
    FilePath::StringType path_string;
    if (!extension_dict->GetString(kPrefPath, &path_string))
      continue;
    FilePath path(path_string);
    if (path.IsAbsolute())
      absolute_keys.insert(*i);
  }
  if (absolute_keys.empty())
    return;

  // Fix these paths.
  DictionaryPrefUpdate update(prefs_, kExtensionsPref);
  const DictionaryValue* update_dict = update.Get();
  for (std::set<std::string>::iterator i = absolute_keys.begin();
       i != absolute_keys.end(); ++i) {
    DictionaryValue* extension_dict = NULL;
    update_dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict);
    FilePath::StringType path_string;
    extension_dict->GetString(kPrefPath, &path_string);
    FilePath path(path_string);
    extension_dict->SetString(kPrefPath,
        MakePathRelative(install_directory_, path));
  }
  SavePrefs();
}

void ExtensionPrefs::MakePathsAbsolute(DictionaryValue* dict) {
  if (!dict || dict->empty())
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    DictionaryValue* extension_dict = NULL;
    if (!dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict)) {
      NOTREACHED();
      continue;
    }

    int location_value;
    if (extension_dict->GetInteger(kPrefLocation, &location_value) &&
        location_value == Extension::LOAD) {
      // Unpacked extensions will already have absolute paths.
      continue;
    }

    FilePath::StringType path_string;
    if (!extension_dict->GetString(kPrefPath, &path_string))
      continue;

    DCHECK(!FilePath(path_string).IsAbsolute());
    extension_dict->SetString(
        kPrefPath, install_directory_.Append(path_string).value());
  }
}

DictionaryValue* ExtensionPrefs::CopyCurrentExtensions() {
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (extensions) {
    DictionaryValue* copy = extensions->DeepCopy();
    MakePathsAbsolute(copy);
    return copy;
  }
  return new DictionaryValue;
}

bool ExtensionPrefs::ReadBooleanFromPref(
    const DictionaryValue* ext, const std::string& pref_key) {
  bool bool_value = false;
  if (!ext->GetBoolean(pref_key, &bool_value))
    return false;

  return bool_value;
}

bool ExtensionPrefs::ReadExtensionPrefBoolean(
    const std::string& extension_id, const std::string& pref_key) {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext) {
    // No such extension yet.
    return false;
  }
  return ReadBooleanFromPref(ext, pref_key);
}

bool ExtensionPrefs::ReadIntegerFromPref(
    const DictionaryValue* ext, const std::string& pref_key, int* out_value) {
  if (!ext->GetInteger(pref_key, out_value))
    return false;

  return out_value != NULL;
}

bool ExtensionPrefs::ReadExtensionPrefInteger(
    const std::string& extension_id, const std::string& pref_key,
    int* out_value) {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext) {
    // No such extension yet.
    return false;
  }
  return ReadIntegerFromPref(ext, pref_key, out_value);
}

bool ExtensionPrefs::ReadExtensionPrefList(
    const std::string& extension_id, const std::string& pref_key,
    const ListValue** out_value) {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  ListValue* out = NULL;
  if (!ext || !ext->GetList(pref_key, &out))
    return false;
  *out_value = out;

  return out_value != NULL;
}

bool ExtensionPrefs::ReadExtensionPrefStringSet(
    const std::string& extension_id,
    const std::string& pref_key,
    std::set<std::string>* result) {
  const ListValue* value = NULL;
  if (!ReadExtensionPrefList(extension_id, pref_key, &value))
    return false;

  result->clear();

  for (size_t i = 0; i < value->GetSize(); ++i) {
    std::string item;
    if (!value->GetString(i, &item))
      return false;
    result->insert(item);
  }

  return true;
}

void ExtensionPrefs::AddToExtensionPrefStringSet(
    const std::string& extension_id,
    const std::string& pref_key,
    const std::set<std::string>& added_value) {
  std::set<std::string> old_value;
  std::set<std::string> new_value;
  ReadExtensionPrefStringSet(extension_id, pref_key, &old_value);

  std::set_union(old_value.begin(), old_value.end(),
                 added_value.begin(), added_value.end(),
                 std::inserter(new_value, new_value.begin()));

  ListValue* value = new ListValue();
  for (std::set<std::string>::const_iterator iter = new_value.begin();
       iter != new_value.end(); ++iter)
    value->Append(Value::CreateStringValue(*iter));

  UpdateExtensionPref(extension_id, pref_key, value);
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionPrefs::SavePrefs() {
  prefs_->ScheduleSavePersistentPrefs();
}

bool ExtensionPrefs::IsBlacklistBitSet(DictionaryValue* ext) {
  return ReadBooleanFromPref(ext, kPrefBlacklist);
}

bool ExtensionPrefs::IsExtensionBlacklisted(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefBlacklist);
}

bool ExtensionPrefs::IsExtensionAllowedByPolicy(
    const std::string& extension_id) {
  std::string string_value;

  const ListValue* blacklist =
      prefs_->GetList(prefs::kExtensionInstallDenyList);
  if (!blacklist || blacklist->empty())
    return true;

  // Check the whitelist first.
  const ListValue* whitelist =
      prefs_->GetList(prefs::kExtensionInstallAllowList);
  if (whitelist) {
    for (ListValue::const_iterator it = whitelist->begin();
         it != whitelist->end(); ++it) {
      if (!(*it)->GetAsString(&string_value))
        LOG(WARNING) << "Failed to read whitelist string.";
      else if (string_value == extension_id)
        return true;
    }
  }

  // Then check the blacklist (the admin blacklist, not the Google blacklist).
  if (blacklist) {
    for (ListValue::const_iterator it = blacklist->begin();
         it != blacklist->end(); ++it) {
      if (!(*it)->GetAsString(&string_value)) {
        LOG(WARNING) << "Failed to read blacklist string.";
      } else {
        if (string_value == "*")
          return false;  // Only whitelisted extensions are allowed.
        if (string_value == extension_id)
          return false;
      }
    }
  }

  return true;
}

bool ExtensionPrefs::DidExtensionEscalatePermissions(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id,
                                  kExtensionDidEscalatePermissions);
}

void ExtensionPrefs::SetDidExtensionEscalatePermissions(
    const Extension* extension, bool did_escalate) {
  UpdateExtensionPref(extension->id(), kExtensionDidEscalatePermissions,
                      Value::CreateBooleanValue(did_escalate));
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionPrefs::UpdateBlacklist(
    const std::set<std::string>& blacklist_set) {
  std::vector<std::string> remove_pref_ids;
  std::set<std::string> used_id_set;
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);

  if (extensions) {
    for (DictionaryValue::key_iterator extension_id = extensions->begin_keys();
         extension_id != extensions->end_keys(); ++extension_id) {
      DictionaryValue* ext;
      if (!extensions->GetDictionaryWithoutPathExpansion(*extension_id, &ext)) {
        NOTREACHED() << "Invalid pref for extension " << *extension_id;
        continue;
      }
      const std::string& id(*extension_id);
      if (blacklist_set.find(id) == blacklist_set.end()) {
        if (!IsBlacklistBitSet(ext)) {
          // This extension is not in blacklist. And it was not blacklisted
          // before.
          continue;
        } else {
          if (ext->size() == 1) {
            // We should remove the entry if the only flag here is blacklist.
            remove_pref_ids.push_back(id);
          } else {
            // Remove the blacklist bit.
            ext->Remove(kPrefBlacklist, NULL);
          }
        }
      } else {
        if (!IsBlacklistBitSet(ext)) {
          // Only set the blacklist if it was not set.
          ext->SetBoolean(kPrefBlacklist, true);
        }
        // Keep the record if this extension is already processed.
        used_id_set.insert(id);
      }
    }
  }

  // Iterate the leftovers to set blacklist in pref
  std::set<std::string>::const_iterator set_itr = blacklist_set.begin();
  for (; set_itr != blacklist_set.end(); ++set_itr) {
    if (used_id_set.find(*set_itr) == used_id_set.end()) {
      UpdateExtensionPref(*set_itr, kPrefBlacklist,
        Value::CreateBooleanValue(true));
    }
  }
  for (unsigned int i = 0; i < remove_pref_ids.size(); ++i) {
    DeleteExtensionPrefs(remove_pref_ids[i]);
  }
  SavePrefs();
  return;
}

namespace {

// Serializes |time| as a string value mapped to |key| in |dictionary|.
void SaveTime(DictionaryValue* dictionary, const char* key, const Time& time) {
  if (!dictionary)
    return;
  std::string string_value = base::Int64ToString(time.ToInternalValue());
  dictionary->SetString(key, string_value);
}

// The opposite of SaveTime. If |key| is not found, this returns an empty Time
// (is_null() will return true).
Time ReadTime(const DictionaryValue* dictionary, const char* key) {
  if (!dictionary)
    return Time();
  std::string string_value;
  int64 value;
  if (dictionary->GetString(key, &string_value)) {
    if (base::StringToInt64(string_value, &value)) {
      return Time::FromInternalValue(value);
    }
  }
  return Time();
}

}  // namespace

Time ExtensionPrefs::LastPingDay(const std::string& extension_id) const {
  DCHECK(Extension::IdIsValid(extension_id));
  return ReadTime(GetExtensionPref(extension_id), kLastPingDay);
}

void ExtensionPrefs::SetLastPingDay(const std::string& extension_id,
                                    const Time& time) {
  DCHECK(Extension::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get(), kLastPingDay, time);
}

Time ExtensionPrefs::BlacklistLastPingDay() const {
  return ReadTime(prefs_->GetDictionary(kExtensionsBlacklistUpdate),
                  kLastPingDay);
}

void ExtensionPrefs::SetBlacklistLastPingDay(const Time& time) {
  DictionaryPrefUpdate update(prefs_, kExtensionsBlacklistUpdate);
  SaveTime(update.Get(), kLastPingDay, time);
}

Time ExtensionPrefs::LastActivePingDay(const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  return ReadTime(GetExtensionPref(extension_id), kLastActivePingDay);
}

void ExtensionPrefs::SetLastActivePingDay(const std::string& extension_id,
                                          const base::Time& time) {
  DCHECK(Extension::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get(), kLastActivePingDay, time);
}

bool ExtensionPrefs::GetActiveBit(const std::string& extension_id) {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kActiveBit, &result))
    return result;
  return false;
}

void ExtensionPrefs::SetActiveBit(const std::string& extension_id,
                                  bool active) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  update.Get()->SetBoolean(kActiveBit, active);
}

bool ExtensionPrefs::GetGrantedPermissions(
    const std::string& extension_id,
    bool* full_access,
    std::set<std::string>* api_permissions,
    ExtensionExtent* host_extent) {
  CHECK(Extension::IdIsValid(extension_id));

  const DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetBoolean(kPrefGrantedPermissionsAll, full_access))
    return false;

  ReadExtensionPrefStringSet(
      extension_id, kPrefGrantedPermissionsAPI, api_permissions);

  std::set<std::string> host_permissions;
  ReadExtensionPrefStringSet(
      extension_id, kPrefGrantedPermissionsHost, &host_permissions);
  bool allow_file_access = AllowFileAccess(extension_id);

  // The granted host permissions contain hosts from the manifest's
  // "permissions" array and from the content script "matches" arrays,
  // so the URLPattern needs to accept valid schemes from both types.
  for (std::set<std::string>::iterator i = host_permissions.begin();
       i != host_permissions.end(); ++i) {
    URLPattern pattern(
        Extension::kValidHostPermissionSchemes |
        UserScript::kValidUserScriptSchemes);

    // Parse without strict checks, so that new strict checks do not
    // fail on a pattern in an installed extension.
    if (URLPattern::PARSE_SUCCESS != pattern.Parse(
            *i, URLPattern::PARSE_LENIENT)) {
      NOTREACHED();  // Corrupt prefs?  Hand editing?
    } else {
      if (!allow_file_access && pattern.MatchesScheme(chrome::kFileScheme)) {
        pattern.set_valid_schemes(
            pattern.valid_schemes() & ~URLPattern::SCHEME_FILE);
      }
      host_extent->AddPattern(pattern);
    }
  }

  return true;
}

void ExtensionPrefs::AddGrantedPermissions(
    const std::string& extension_id,
    const bool full_access,
    const std::set<std::string>& api_permissions,
    const ExtensionExtent& host_extent) {
  CHECK(Extension::IdIsValid(extension_id));

  UpdateExtensionPref(extension_id, kPrefGrantedPermissionsAll,
                      Value::CreateBooleanValue(full_access));

  if (!api_permissions.empty()) {
    AddToExtensionPrefStringSet(
        extension_id, kPrefGrantedPermissionsAPI, api_permissions);
  }

  if (!host_extent.is_empty()) {
    std::set<std::string> host_permissions;
    ExtentToStringSet(host_extent, &host_permissions);

    AddToExtensionPrefStringSet(
        extension_id, kPrefGrantedPermissionsHost, host_permissions);
  }

  SavePrefs();
}

bool ExtensionPrefs::IsIncognitoEnabled(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefIncognitoEnabled);
}

void ExtensionPrefs::SetIsIncognitoEnabled(const std::string& extension_id,
                                           bool enabled) {
  UpdateExtensionPref(extension_id, kPrefIncognitoEnabled,
                      Value::CreateBooleanValue(enabled));
  SavePrefs();
}

bool ExtensionPrefs::AllowFileAccess(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefAllowFileAccess);
}

void ExtensionPrefs::SetAllowFileAccess(const std::string& extension_id,
                                        bool allow) {
  UpdateExtensionPref(extension_id, kPrefAllowFileAccess,
                      Value::CreateBooleanValue(allow));
  SavePrefs();
}

bool ExtensionPrefs::HasAllowFileAccessSetting(
    const std::string& extension_id) const {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  return ext && ext->HasKey(kPrefAllowFileAccess);
}

ExtensionPrefs::LaunchType ExtensionPrefs::GetLaunchType(
    const std::string& extension_id,
    ExtensionPrefs::LaunchType default_pref_value) {
  int value = -1;
  LaunchType result = LAUNCH_REGULAR;

  if (ReadExtensionPrefInteger(extension_id, kPrefLaunchType, &value) &&
     (value == LAUNCH_PINNED ||
      value == LAUNCH_REGULAR ||
      value == LAUNCH_FULLSCREEN ||
      value == LAUNCH_WINDOW)) {
    result = static_cast<LaunchType>(value);
  } else {
    result = default_pref_value;
  }
  #if defined(OS_MACOSX)
    // App windows are not yet supported on mac.  Pref sync could make
    // the launch type LAUNCH_WINDOW, even if there is no UI to set it
    // on mac.
    if (result == LAUNCH_WINDOW)
      result = LAUNCH_REGULAR;
  #endif

  return result;
}

extension_misc::LaunchContainer ExtensionPrefs::GetLaunchContainer(
    const Extension* extension,
    ExtensionPrefs::LaunchType default_pref_value) {
  extension_misc::LaunchContainer manifest_launch_container =
      extension->launch_container();

  const extension_misc::LaunchContainer kInvalidLaunchContainer =
      static_cast<extension_misc::LaunchContainer>(-1);

  extension_misc::LaunchContainer result = kInvalidLaunchContainer;

  if (manifest_launch_container == extension_misc::LAUNCH_PANEL) {
    // Apps with app.launch.container = 'panel' should always
    // open in a panel.
    result = extension_misc::LAUNCH_PANEL;

  } else if (manifest_launch_container == extension_misc::LAUNCH_TAB) {
    // Look for prefs that indicate the user's choice of launch
    // container.  The app's menu on the NTP provides a UI to set
    // this preference.  If no preference is set, |default_pref_value|
    // is used.
    ExtensionPrefs::LaunchType prefs_launch_type =
        GetLaunchType(extension->id(), default_pref_value);

    if (prefs_launch_type == ExtensionPrefs::LAUNCH_WINDOW) {
      // If the pref is set to launch a window (or no pref is set, and
      // window opening is the default), make the container a window.
      result = extension_misc::LAUNCH_WINDOW;

    } else {
      // All other launch types (tab, pinned, fullscreen) are
      // implemented as tabs in a window.
      result = extension_misc::LAUNCH_TAB;
    }
  } else {
    // If a new value for app.launch.container is added, logic
    // for it should be added here.  extension_misc::LAUNCH_WINDOW
    // is not present because there is no way to set it in a manifest.
    NOTREACHED() << manifest_launch_container;
  }

  // All paths should set |result|.
  if (result == kInvalidLaunchContainer) {
    DLOG(FATAL) << "Failed to set a launch container.";
    result = extension_misc::LAUNCH_TAB;
  }

  return result;
}

void ExtensionPrefs::SetLaunchType(const std::string& extension_id,
                                   LaunchType launch_type) {
  UpdateExtensionPref(extension_id, kPrefLaunchType,
      Value::CreateIntegerValue(static_cast<int>(launch_type)));
  SavePrefs();
}

bool ExtensionPrefs::IsExternalExtensionUninstalled(
    const std::string& id) const {
  const DictionaryValue* extension = GetExtensionPref(id);
  if (!extension)
    return false;
  int state = 0;
  return extension->GetInteger(kPrefState, &state) &&
         state == static_cast<int>(Extension::EXTERNAL_EXTENSION_UNINSTALLED);
}

std::vector<std::string> ExtensionPrefs::GetToolbarOrder() {
  ExtensionPrefs::ExtensionIdSet extension_ids;
  const ListValue* toolbar_order = prefs_->GetList(kExtensionToolbar);
  if (toolbar_order) {
    for (size_t i = 0; i < toolbar_order->GetSize(); ++i) {
      std::string extension_id;
      if (toolbar_order->GetString(i, &extension_id))
        extension_ids.push_back(extension_id);
    }
  }
  return extension_ids;
}

void ExtensionPrefs::SetToolbarOrder(
    const std::vector<std::string>& extension_ids) {
  ListPrefUpdate update(prefs_, kExtensionToolbar);
  ListValue* toolbar_order = update.Get();
  toolbar_order->Clear();
  for (std::vector<std::string>::const_iterator iter = extension_ids.begin();
       iter != extension_ids.end(); ++iter) {
    toolbar_order->Append(new StringValue(*iter));
  }
  SavePrefs();
}

void ExtensionPrefs::OnExtensionInstalled(
    const Extension* extension, Extension::State initial_state,
    bool initial_incognito_enabled) {
  const std::string& id = extension->id();
  CHECK(Extension::IdIsValid(id));
  ScopedExtensionPrefUpdate update(prefs_, id);
  DictionaryValue* extension_dict = update.Get();
  const base::Time install_time = GetCurrentTime();
  extension_dict->Set(kPrefState, Value::CreateIntegerValue(initial_state));
  extension_dict->Set(kPrefIncognitoEnabled,
                      Value::CreateBooleanValue(initial_incognito_enabled));
  extension_dict->Set(kPrefLocation,
                      Value::CreateIntegerValue(extension->location()));
  extension_dict->Set(kPrefInstallTime,
                      Value::CreateStringValue(
                          base::Int64ToString(install_time.ToInternalValue())));
  extension_dict->Set(kPrefPreferences, new DictionaryValue());

  FilePath::StringType path = MakePathRelative(install_directory_,
      extension->path());
  extension_dict->Set(kPrefPath, Value::CreateStringValue(path));
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (extension->location() != Extension::LOAD) {
    extension_dict->Set(kPrefManifest,
                        extension->manifest_value()->DeepCopy());
  }
  extension_dict->Set(kPrefAppLaunchIndex,
                      Value::CreateIntegerValue(GetNextAppLaunchIndex()));
  extension_pref_value_map_->RegisterExtension(
      id, install_time, initial_state == Extension::ENABLED);
  SavePrefs();
}

void ExtensionPrefs::OnExtensionUninstalled(const std::string& extension_id,
                                            const Extension::Location& location,
                                            bool external_uninstall) {
  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall && Extension::IsExternalLocation(location)) {
    UpdateExtensionPref(extension_id, kPrefState,
                        Value::CreateIntegerValue(
                            Extension::EXTERNAL_EXTENSION_UNINSTALLED));
    SavePrefs();
    extension_pref_value_map_->SetExtensionState(extension_id, false);
  } else {
    DeleteExtensionPrefs(extension_id);
  }
}

Extension::State ExtensionPrefs::GetExtensionState(
    const std::string& extension_id) const {
  const DictionaryValue* extension = GetExtensionPref(extension_id);

  // If the extension doesn't have a pref, it's a --load-extension.
  if (!extension)
    return Extension::ENABLED;

  int state = -1;
  if (!extension->GetInteger(kPrefState, &state) ||
      state < 0 || state >= Extension::NUM_STATES) {
    LOG(ERROR) << "Bad or missing pref 'state' for extension '"
               << extension_id << "'";
    return Extension::ENABLED;
  }
  return static_cast<Extension::State>(state);
}

void ExtensionPrefs::SetExtensionState(const Extension* extension,
                                       Extension::State state) {
  UpdateExtensionPref(extension->id(), kPrefState,
                      Value::CreateIntegerValue(state));
  SavePrefs();

  bool enabled = (state == Extension::ENABLED);
  extension_pref_value_map_->SetExtensionState(extension->id(), enabled);
}

bool ExtensionPrefs::GetBrowserActionVisibility(const Extension* extension) {
  const DictionaryValue* extension_prefs = GetExtensionPref(extension->id());
  if (!extension_prefs)
    return true;
  bool visible = false;
  if (!extension_prefs->GetBoolean(kBrowserActionVisible, &visible) || visible)
    return true;

  return false;
}

void ExtensionPrefs::SetBrowserActionVisibility(const Extension* extension,
                                                bool visible) {
  if (GetBrowserActionVisibility(extension) == visible)
    return;

  UpdateExtensionPref(extension->id(), kBrowserActionVisible,
                      Value::CreateBooleanValue(visible));
  SavePrefs();

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      Source<ExtensionPrefs>(this),
      Details<const Extension>(extension));
}

std::string ExtensionPrefs::GetVersionString(const std::string& extension_id) {
  const DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return std::string();

  std::string version;
  if (!extension->GetString(kPrefVersion, &version)) {
    LOG(ERROR) << "Bad or missing pref 'version' for extension '"
               << extension_id << "'";
  }

  return version;
}

void ExtensionPrefs::UpdateManifest(const Extension* extension) {
  if (extension->location() != Extension::LOAD) {
    const DictionaryValue* extension_dict = GetExtensionPref(extension->id());
    if (!extension_dict)
      return;
    DictionaryValue* old_manifest = NULL;
    bool update_required =
        !extension_dict->GetDictionary(kPrefManifest, &old_manifest) ||
        !extension->manifest_value()->Equals(old_manifest);
    if (update_required) {
      UpdateExtensionPref(extension->id(), kPrefManifest,
                          extension->manifest_value()->DeepCopy());
    }
    SavePrefs();
  }
}

FilePath ExtensionPrefs::GetExtensionPath(const std::string& extension_id) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict || dict->empty())
    return FilePath();

  std::string path;
  if (!dict->GetString(extension_id + "." + kPrefPath, &path))
    return FilePath();

  return install_directory_.Append(FilePath::FromWStringHack(UTF8ToWide(path)));
}

void ExtensionPrefs::UpdateExtensionPref(const std::string& extension_id,
                                         const std::string& key,
                                         Value* data_value) {
  if (!Extension::IdIsValid(extension_id)) {
    NOTREACHED() << "Invalid extension_id " << extension_id;
    return;
  }
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension = update.Get();
  extension->Set(key, data_value);
}

void ExtensionPrefs::DeleteExtensionPrefs(const std::string& extension_id) {
  DictionaryPrefUpdate update(prefs_, kExtensionsPref);
  DictionaryValue* dict = update.Get();
  if (dict->HasKey(extension_id)) {
    dict->Remove(extension_id, NULL);
    SavePrefs();
  }
  extension_pref_value_map_->UnregisterExtension(extension_id);
}

const DictionaryValue* ExtensionPrefs::GetExtensionPref(
    const std::string& extension_id) const {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict)
    return NULL;
  DictionaryValue* extension = NULL;
  dict->GetDictionary(extension_id, &extension);
  return extension;
}

// Helper function for GetInstalledExtensionsInfo.
static ExtensionInfo* GetInstalledExtensionInfoImpl(
    DictionaryValue* extension_data,
    DictionaryValue::key_iterator extension_id) {
  DictionaryValue* ext;
  if (!extension_data->GetDictionaryWithoutPathExpansion(*extension_id, &ext)) {
    LOG(WARNING) << "Invalid pref for extension " << *extension_id;
    NOTREACHED();
    return NULL;
  }
  if (ext->HasKey(kPrefBlacklist)) {
    bool is_blacklisted = false;
    if (!ext->GetBoolean(kPrefBlacklist, &is_blacklisted)) {
      NOTREACHED() << "Invalid blacklist pref:" << *extension_id;
      return NULL;
    }
    if (is_blacklisted) {
      return NULL;
    }
  }
  int state_value;
  if (!ext->GetInteger(kPrefState, &state_value)) {
    // This can legitimately happen if we store preferences for component
    // extensions.
    return NULL;
  }
  if (state_value == Extension::EXTERNAL_EXTENSION_UNINSTALLED) {
    LOG(WARNING) << "External extension with id " << *extension_id
                 << " has been uninstalled by the user";
    return NULL;
  }
  FilePath::StringType path;
  if (!ext->GetString(kPrefPath, &path)) {
    return NULL;
  }
  int location_value;
  if (!ext->GetInteger(kPrefLocation, &location_value)) {
    return NULL;
  }

  // Only the following extension types can be installed permanently in the
  // preferences.
  Extension::Location location =
      static_cast<Extension::Location>(location_value);
  if (location != Extension::INTERNAL &&
      location != Extension::LOAD &&
      !Extension::IsExternalLocation(location)) {
    NOTREACHED();
    return NULL;
  }

  DictionaryValue* manifest = NULL;
  if (location != Extension::LOAD &&
      !ext->GetDictionary(kPrefManifest, &manifest)) {
    LOG(WARNING) << "Missing manifest for extension " << *extension_id;
    // Just a warning for now.
  }

  return new ExtensionInfo(manifest, *extension_id, FilePath(path), location);
}

ExtensionPrefs::ExtensionsInfo* ExtensionPrefs::GetInstalledExtensionsInfo() {
  scoped_ptr<DictionaryValue> extension_data(CopyCurrentExtensions());

  ExtensionsInfo* extensions_info = new ExtensionsInfo;

  for (DictionaryValue::key_iterator extension_id(
           extension_data->begin_keys());
       extension_id != extension_data->end_keys(); ++extension_id) {
    if (!Extension::IdIsValid(*extension_id))
      continue;

    ExtensionInfo* info = GetInstalledExtensionInfoImpl(extension_data.get(),
                                                        extension_id);
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info));
  }

  return extensions_info;
}

ExtensionInfo* ExtensionPrefs::GetInstalledExtensionInfo(
    const std::string& extension_id) {
  scoped_ptr<DictionaryValue> extension_data(CopyCurrentExtensions());

  for (DictionaryValue::key_iterator extension_iter(
           extension_data->begin_keys());
       extension_iter != extension_data->end_keys(); ++extension_iter) {
    if (*extension_iter == extension_id) {
      return GetInstalledExtensionInfoImpl(extension_data.get(),
                                           extension_iter);
    }
  }

  return NULL;
}

void ExtensionPrefs::SetIdleInstallInfo(const std::string& extension_id,
                                        const FilePath& crx_path,
                                        const std::string& version,
                                        const base::Time& fetch_time) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_prefs = update.Get();
  if (!extension_prefs) {
    NOTREACHED();
    return;
  }
  extension_prefs->Remove(kIdleInstallInfo, NULL);
  DictionaryValue* info = new DictionaryValue();
  info->SetString(kIdleInstallInfoCrxPath, crx_path.value());
  info->SetString(kIdleInstallInfoVersion, version);
  info->SetString(kIdleInstallInfoFetchTime,
                  base::Int64ToString(fetch_time.ToInternalValue()));
  extension_prefs->Set(kIdleInstallInfo, info);
  SavePrefs();
}

bool ExtensionPrefs::RemoveIdleInstallInfo(const std::string& extension_id) {
  if (!GetExtensionPref(extension_id))
    return false;
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_prefs = update.Get();
  bool result = extension_prefs->Remove(kIdleInstallInfo, NULL);
  SavePrefs();
  return result;
}

bool ExtensionPrefs::GetIdleInstallInfo(const std::string& extension_id,
                                        FilePath* crx_path,
                                        std::string* version,
                                        base::Time* fetch_time) {
  const DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return false;

  // Do all the reads from the prefs together, and don't do any assignment
  // to the out parameters unless all the reads succeed.
  DictionaryValue* info = NULL;
  if (!extension_prefs->GetDictionary(kIdleInstallInfo, &info))
    return false;

  FilePath::StringType path_string;
  if (!info->GetString(kIdleInstallInfoCrxPath, &path_string))
    return false;

  std::string tmp_version;
  if (!info->GetString(kIdleInstallInfoVersion, &tmp_version))
    return false;

  std::string fetch_time_string;
  if (!info->GetString(kIdleInstallInfoFetchTime, &fetch_time_string))
    return false;

  int64 fetch_time_value;
  if (!base::StringToInt64(fetch_time_string, &fetch_time_value))
    return false;

  if (crx_path)
    *crx_path = FilePath(path_string);

  if (version)
    *version = tmp_version;

  if (fetch_time)
    *fetch_time = base::Time::FromInternalValue(fetch_time_value);

  return true;
}

std::set<std::string> ExtensionPrefs::GetIdleInstallInfoIds() {
  std::set<std::string> result;

  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions)
    return result;

  for (DictionaryValue::key_iterator iter = extensions->begin_keys();
       iter != extensions->end_keys(); ++iter) {
    const std::string& id(*iter);
    if (!Extension::IdIsValid(id)) {
      NOTREACHED();
      continue;
    }

    const DictionaryValue* extension_prefs = GetExtensionPref(id);
    if (!extension_prefs)
      continue;

    if (extension_prefs->GetDictionary(kIdleInstallInfo, NULL))
      result.insert(id);
  }
  return result;
}

bool ExtensionPrefs::GetWebStoreLogin(std::string* result) {
  if (prefs_->HasPrefPath(kWebStoreLogin)) {
    *result = prefs_->GetString(kWebStoreLogin);
    return true;
  }
  return false;
}

void ExtensionPrefs::SetWebStoreLogin(const std::string& login) {
  prefs_->SetString(kWebStoreLogin, login);
  SavePrefs();
}

int ExtensionPrefs::GetAppLaunchIndex(const std::string& extension_id) {
  int value;
  if (ReadExtensionPrefInteger(extension_id, kPrefAppLaunchIndex, &value))
    return value;

  return -1;
}

void ExtensionPrefs::SetAppLaunchIndex(const std::string& extension_id,
                                       int index) {
  DCHECK_GE(index, 0);
  UpdateExtensionPref(extension_id, kPrefAppLaunchIndex,
                      Value::CreateIntegerValue(index));
  SavePrefs();
}

int ExtensionPrefs::GetNextAppLaunchIndex() {
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions)
    return 0;

  int max_value = -1;
  for (DictionaryValue::key_iterator extension_id = extensions->begin_keys();
       extension_id != extensions->end_keys(); ++extension_id) {
    int value = GetAppLaunchIndex(*extension_id);
    if (value > max_value)
      max_value = value;
  }
  return max_value + 1;
}

void ExtensionPrefs::SetAppLauncherOrder(
    const std::vector<std::string>& extension_ids) {
  for (size_t i = 0; i < extension_ids.size(); ++i)
    SetAppLaunchIndex(extension_ids.at(i), i);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_LAUNCHER_REORDERED,
      Source<ExtensionPrefs>(this),
      NotificationService::NoDetails());
}

int ExtensionPrefs::GetPageIndex(const std::string& extension_id) {
  int value;
  if (ReadExtensionPrefInteger(extension_id, kPrefPageIndex, &value))
    return value;

  return -1;
}

void ExtensionPrefs::SetPageIndex(const std::string& extension_id, int index) {
  CHECK_GE(index, 0);
  UpdateExtensionPref(extension_id, kPrefPageIndex,
                      Value::CreateIntegerValue(index));
  SavePrefs();
}

bool ExtensionPrefs::WasAppDraggedByUser(const std::string& extension_id) {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  if (!dictionary) {
    NOTREACHED();
    return false;
  }

  return ReadBooleanFromPref(dictionary, kPrefUserDraggedApp);
}

void ExtensionPrefs::SetAppDraggedByUser(const std::string& extension_id) {
  if (!GetExtensionPref(extension_id)) {
    NOTREACHED();
    return;
  }

  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* dictionary = update.Get();
  dictionary->SetBoolean(kPrefUserDraggedApp, true);
  SavePrefs();
}

void ExtensionPrefs::SetUpdateUrlData(const std::string& extension_id,
                                      const std::string& data) {
  if (!GetExtensionPref(extension_id)) {
    NOTREACHED();
    return;
  }

  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* dictionary = update.Get();
  dictionary->SetString(kUpdateUrlData, data);
  SavePrefs();
}

std::string ExtensionPrefs::GetUpdateUrlData(const std::string& extension_id) {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  if (!dictionary)
    return std::string();

  std::string data;
  dictionary->GetString(kUpdateUrlData, &data);
  return data;
}

base::Time ExtensionPrefs::GetCurrentTime() const {
  return base::Time::Now();
}

base::Time ExtensionPrefs::GetInstallTime(
    const std::string& extension_id) const {
  const DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension) {
    NOTREACHED();
    return base::Time();
  }
  std::string install_time_str;
  if (!extension->GetString(kPrefInstallTime, &install_time_str))
    return base::Time();
  int64 install_time_i64 = 0;
  if (!base::StringToInt64(install_time_str, &install_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(install_time_i64);
}

void ExtensionPrefs::GetExtensions(ExtensionIdSet* out) {
  CHECK(out);

  scoped_ptr<ExtensionsInfo> extensions_info(GetInstalledExtensionsInfo());

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    out->push_back(info->extension_id);
  }
}

void ExtensionPrefs::FixMissingPrefs(const ExtensionIdSet& extension_ids) {
  // Fix old entries that did not get an installation time entry when they
  // were installed or don't have a preferences field.
  bool persist_required = false;
  for (ExtensionIdSet::const_iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    if (GetInstallTime(*ext_id) == base::Time()) {
      LOG(INFO) << "Could not parse installation time of extension "
                << *ext_id << ". It was probably installed before setting "
                << kPrefInstallTime << " was introduced. Updating "
                << kPrefInstallTime << " to the current time.";
      const base::Time install_time = GetCurrentTime();
      ScopedExtensionPrefUpdate update(prefs_, *ext_id);
      DictionaryValue* extension = update.Get();
      extension->Set(kPrefInstallTime,
                     Value::CreateStringValue(
                         base::Int64ToString(install_time.ToInternalValue())));
      persist_required = true;
    }
  }
  if (persist_required)
    SavePrefs();
}

const DictionaryValue* ExtensionPrefs::GetExtensionControlledPrefs(
    const std::string& extension_id) const {
  std::string key = extension_id + std::string(".") + kPrefPreferences;
  DictionaryValue* preferences = NULL;
  // First try the regular lookup.
  {
    const DictionaryValue* source_dict = prefs_->GetDictionary(kExtensionsPref);
    if (source_dict->GetDictionary(key, &preferences))
      return preferences;
  }
  // And then create a dictionary if it did not exist before.
  {
    DictionaryPrefUpdate update(prefs_, kExtensionsPref);
    update.Get()->Set(key, new DictionaryValue);
  }
  const DictionaryValue* source_dict = prefs_->GetDictionary(kExtensionsPref);
  source_dict->GetDictionary(key, &preferences);
  return preferences;
}

void ExtensionPrefs::InitPrefStore() {
  // When this is called, the PrefService is initialized and provides access
  // to the user preferences stored in a JSON file.
  ExtensionIdSet extension_ids;
  GetExtensions(&extension_ids);
  // Create empty preferences dictionary for each extension (these dictionaries
  // are pruned when persisting the preferneces to disk).
  {
    DictionaryPrefUpdate update(prefs_, kExtensionsPref);
    const DictionaryValue* source_dict = prefs_->GetDictionary(kExtensionsPref);
    for (ExtensionIdSet::iterator ext_id = extension_ids.begin();
         ext_id != extension_ids.end(); ++ext_id) {
      std::string key = *ext_id + std::string(".") + kPrefPreferences;
      if (!source_dict->GetDictionary(key, NULL))
        update.Get()->Set(key, new DictionaryValue);
    }
  }

  FixMissingPrefs(extension_ids);
  // Store extension controlled preference values in the
  // |extension_pref_value_map_|, which then informs the subscribers
  // (ExtensionPrefStores) about the winning values.
  for (ExtensionIdSet::iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    extension_pref_value_map_->RegisterExtension(
        *ext_id,
        GetInstallTime(*ext_id),
        GetExtensionState(*ext_id) == Extension::ENABLED);

    const DictionaryValue* prefs = GetExtensionControlledPrefs(*ext_id);
    for (DictionaryValue::key_iterator i = prefs->begin_keys();
         i != prefs->end_keys(); ++i) {
      Value* value;
      if (!prefs->GetWithoutPathExpansion(*i, &value))
        continue;
      extension_pref_value_map_->SetExtensionPref(
          *ext_id, *i, false, value->DeepCopy());
    }
  }

  extension_pref_value_map_->NotifyInitializationCompleted();
}


void ExtensionPrefs::SetExtensionControlledPref(const std::string& extension_id,
                                                const std::string& pref_key,
                                                bool incognito,
                                                Value* value) {
#ifndef NDEBUG
  const PrefService::Preference* pref =
      pref_service()->FindPreference(pref_key.c_str());
  DCHECK(pref) << "Extension controlled preference key " << pref_key
               << " not registered.";
  DCHECK_EQ(pref->GetType(), value->GetType())
      << "Extension controlled preference " << pref_key << " has wrong type.";
#endif

  if (!incognito) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ScopedExtensionControlledPrefUpdate update(prefs_, extension_id);
    DictionaryValue* dict = update.Get();
    dict->SetWithoutPathExpansion(pref_key, value->DeepCopy());
    pref_service()->ScheduleSavePersistentPrefs();
  }

  extension_pref_value_map_->SetExtensionPref(
      extension_id, pref_key, incognito, value);
}

void ExtensionPrefs::RemoveExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    bool incognito) {
  DCHECK(pref_service()->FindPreference(pref_key.c_str()))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  if (!incognito) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ScopedExtensionControlledPrefUpdate update(prefs_, extension_id);
    DictionaryValue* dict = update.Get();
    dict->RemoveWithoutPathExpansion(pref_key, NULL);
    pref_service()->ScheduleSavePersistentPrefs();
  }

  extension_pref_value_map_->RemoveExtensionPref(
      extension_id, pref_key, incognito);
}

bool ExtensionPrefs::CanExtensionControlPref(const std::string& extension_id,
                                             const std::string& pref_key,
                                             bool incognito) {
  DCHECK(pref_service()->FindPreference(pref_key.c_str()))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map_->CanExtensionControlPref(extension_id,
                                                            pref_key,
                                                            incognito);
}

bool ExtensionPrefs::DoesExtensionControlPref(const std::string& extension_id,
                                              const std::string& pref_key,
                                              bool incognito) {
  DCHECK(pref_service()->FindPreference(pref_key.c_str()))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map_->DoesExtensionControlPref(extension_id,
                                                             pref_key,
                                                             incognito);
}

bool ExtensionPrefs::HasIncognitoPrefValue(const std::string& pref_key) {
  bool has_incognito_pref_value = false;
  extension_pref_value_map_->GetEffectivePrefValue(pref_key,
                                                   true,
                                                   &has_incognito_pref_value);
  return has_incognito_pref_value;
}

// static
void ExtensionPrefs::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(kExtensionsPref);
  prefs->RegisterListPref(kExtensionToolbar);
  prefs->RegisterIntegerPref(prefs::kExtensionToolbarSize, -1);
  prefs->RegisterDictionaryPref(kExtensionsBlacklistUpdate);
  prefs->RegisterListPref(prefs::kExtensionInstallAllowList);
  prefs->RegisterListPref(prefs::kExtensionInstallDenyList);
  prefs->RegisterListPref(prefs::kExtensionInstallForceList);
  prefs->RegisterStringPref(kWebStoreLogin, std::string() /* default_value */);
}
