// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

using base::Time;

namespace {

// Preferences keys

// A preference that keeps track of per-extension settings. This is a dictionary
// object read from the Preferences file, keyed off of extension id's.
const wchar_t kExtensionsPref[] = L"extensions.settings";

// Where an extension was installed from. (see Extension::Location)
const wchar_t kPrefLocation[] = L"location";

// Enabled, disabled, killed, etc. (see Extension::State)
const wchar_t kPrefState[] = L"state";

// The path to the current version's manifest file.
const wchar_t kPrefPath[] = L"path";

// The dictionary containing the extension's manifest.
const wchar_t kPrefManifest[] = L"manifest";

// The version number.
const wchar_t kPrefVersion[] = L"manifest.version";

// Indicates if an extension is blacklisted:
const wchar_t kPrefBlacklist[] = L"blacklist";

// Indicates whether to show an install warning when the user enables.
const wchar_t kExtensionDidEscalatePermissions[] = L"install_warning_on_enable";

// A preference that tracks extension shelf configuration.  This is a list
// object read from the Preferences file, containing a list of toolstrip URLs.
const wchar_t kExtensionShelf[] = L"extensions.shelf";

// A preference that tracks browser action toolbar configuration. This is a list
// object stored in the Preferences file. The extensions are stored by ID.
const wchar_t kExtensionToolbar[] = L"extensions.toolbar";

// The key for a serialized Time value indicating the start of the day (from the
// server's perspective) an extension last included a "ping" parameter during
// its update check.
const wchar_t kLastPingDay[] = L"lastpingday";

// Path for settings specific to blacklist update.
const wchar_t kExtensionsBlacklistUpdate[] = L"extensions.blacklistupdate";

// Path and sub-keys for the idle install info dictionary preference.
const wchar_t kIdleInstallInfo[] = L"idle_install_info";
const wchar_t kIdleInstallInfoCrxPath[] = L"crx_path";
const wchar_t kIdleInstallInfoVersion[] = L"version";
const wchar_t kIdleInstallInfoFetchTime[] = L"fetch_time";


// A preference that, if true, will allow this extension to run in incognito
// mode.
const wchar_t kPrefIncognitoEnabled[] = L"incognito";

// A preference to control whether an extension is allowed to inject script in
// pages with file URLs.
const wchar_t kPrefAllowFileAccess[] = L"allowFileAccess";
}

////////////////////////////////////////////////////////////////////////////////

namespace {

// TODO(asargent) - This is cleanup code for a key that was introduced into
// the extensions.settings sub-dictionary which wasn't a valid extension
// id. We can remove this in a couple of months. (See http://crbug.com/40017
// and http://crbug.com/39745 for more details).
static void CleanupBadExtensionKeys(PrefService* prefs) {
  DictionaryValue* dictionary = prefs->GetMutableDictionary(kExtensionsPref);
  std::set<std::wstring> bad_keys;
  for (DictionaryValue::key_iterator i = dictionary->begin_keys();
       i != dictionary->end_keys(); ++i) {
    const std::wstring key_name = *i;
    if (!Extension::IdIsValid(WideToASCII(key_name))) {
      bad_keys.insert(key_name);
    }
  }
  bool dirty = false;
  for (std::set<std::wstring>::iterator i = bad_keys.begin();
       i != bad_keys.end(); ++i) {
    dirty = true;
    dictionary->Remove(*i, NULL);
  }
  if (dirty)
    prefs->ScheduleSavePersistentPrefs();
}

}  // namespace

ExtensionPrefs::ExtensionPrefs(PrefService* prefs, const FilePath& root_dir)
    : prefs_(prefs),
      install_directory_(root_dir) {
  // TODO(asargent) - Remove this in a couple of months. (See comment above
  // CleanupBadExtensionKeys).
  CleanupBadExtensionKeys(prefs);

  MakePathsRelative();
}

static FilePath::StringType MakePathRelative(const FilePath& parent,
                                             const FilePath& child,
                                             bool *dirty) {
  if (!parent.IsParent(child))
    return child.value();

  if (dirty)
    *dirty = true;
  FilePath::StringType retval = child.value().substr(
      parent.value().length());
  if (FilePath::IsSeparator(retval[0]))
    return retval.substr(1);
  else
    return retval;
}

void ExtensionPrefs::MakePathsRelative() {
  bool dirty = false;
  const DictionaryValue* dict = prefs_->GetMutableDictionary(kExtensionsPref);
  if (!dict || dict->empty())
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    DictionaryValue* extension_dict;
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
    if (path.IsAbsolute()) {
      extension_dict->SetString(kPrefPath,
          MakePathRelative(install_directory_, path, &dirty));
    }
  }
  if (dirty)
    prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionPrefs::MakePathsAbsolute(DictionaryValue* dict) {
  if (!dict || dict->empty())
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    DictionaryValue* extension_dict;
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
    DictionaryValue* copy =
        static_cast<DictionaryValue*>(extensions->DeepCopy());
    MakePathsAbsolute(copy);
    return copy;
  }
  return new DictionaryValue;
}

bool ExtensionPrefs::ReadBooleanFromPref(
    DictionaryValue* ext, const std::wstring& pref_key) {
  if (!ext->HasKey(pref_key)) return false;
  bool bool_value = false;
  if (!ext->GetBoolean(pref_key, &bool_value)) {
    NOTREACHED() << "Failed to fetch " << pref_key << " flag.";
    // In case we could not fetch the flag, we treat it as false.
    return false;
  }
  return bool_value;
}

bool ExtensionPrefs::ReadExtensionPrefBoolean(
    const std::string& extension_id, const std::wstring& pref_key) {
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions)
    return false;

  DictionaryValue* ext = NULL;
  if (!extensions->GetDictionary(ASCIIToWide(extension_id), &ext)) {
    // No such extension yet.
    return false;
  }
  return ReadBooleanFromPref(ext, pref_key);
}

bool ExtensionPrefs::IsBlacklistBitSet(DictionaryValue* ext) {
  return ReadBooleanFromPref(ext, kPrefBlacklist);
}

bool ExtensionPrefs::IsExtensionBlacklisted(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefBlacklist);
}

bool ExtensionPrefs::DidExtensionEscalatePermissions(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id,
                                  kExtensionDidEscalatePermissions);
}

void ExtensionPrefs::SetDidExtensionEscalatePermissions(
    Extension* extension, bool did_escalate) {
  UpdateExtensionPref(extension->id(), kExtensionDidEscalatePermissions,
                      Value::CreateBooleanValue(did_escalate));
  prefs_->SavePersistentPrefs();
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
      std::string id = WideToASCII(*extension_id);
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
  // Update persistent registry
  prefs_->ScheduleSavePersistentPrefs();
  return;
}

Time ExtensionPrefs::LastPingDayImpl(const DictionaryValue* dictionary) const {
  if (dictionary && dictionary->HasKey(kLastPingDay)) {
    std::string string_value;
    int64 value;
    dictionary->GetString(kLastPingDay, &string_value);
    if (StringToInt64(string_value, &value)) {
      return Time::FromInternalValue(value);
    }
  }
  return Time();
}

void ExtensionPrefs::SetLastPingDayImpl(const Time& time,
                                        DictionaryValue* dictionary) {
  if (!dictionary) {
    NOTREACHED();
    return;
  }
  std::string value = Int64ToString(time.ToInternalValue());
  dictionary->SetString(kLastPingDay, value);
  prefs_->ScheduleSavePersistentPrefs();
}

Time ExtensionPrefs::LastPingDay(const std::string& extension_id) const {
  DCHECK(Extension::IdIsValid(extension_id));
  return LastPingDayImpl(GetExtensionPref(extension_id));
}

Time ExtensionPrefs::BlacklistLastPingDay() const {
  return LastPingDayImpl(prefs_->GetDictionary(kExtensionsBlacklistUpdate));
}

void ExtensionPrefs::SetLastPingDay(const std::string& extension_id,
                                    const Time& time) {
  DCHECK(Extension::IdIsValid(extension_id));
  SetLastPingDayImpl(time, GetExtensionPref(extension_id));
}

void ExtensionPrefs::SetBlacklistLastPingDay(const Time& time) {
  SetLastPingDayImpl(time,
                     prefs_->GetMutableDictionary(kExtensionsBlacklistUpdate));
}

bool ExtensionPrefs::IsIncognitoEnabled(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefIncognitoEnabled);
}

void ExtensionPrefs::SetIsIncognitoEnabled(const std::string& extension_id,
                                           bool enabled) {
  UpdateExtensionPref(extension_id, kPrefIncognitoEnabled,
                      Value::CreateBooleanValue(enabled));
  prefs_->SavePersistentPrefs();
}

bool ExtensionPrefs::AllowFileAccess(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefAllowFileAccess);
}

void ExtensionPrefs::SetAllowFileAccess(const std::string& extension_id,
                                        bool allow) {
  UpdateExtensionPref(extension_id, kPrefAllowFileAccess,
                      Value::CreateBooleanValue(allow));
  prefs_->SavePersistentPrefs();
}

void ExtensionPrefs::GetKilledExtensionIds(std::set<std::string>* killed_ids) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict || dict->empty())
    return;

  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    std::wstring key_name = *i;
    if (!Extension::IdIsValid(WideToASCII(key_name))) {
      LOG(WARNING) << "Invalid external extension ID encountered: "
                   << WideToASCII(key_name);
      continue;
    }

    DictionaryValue* extension;
    if (!dict->GetDictionary(key_name, &extension)) {
      NOTREACHED();
      continue;
    }

    // Check to see if the extension has been killed.
    int state;
    if (extension->GetInteger(kPrefState, &state) &&
        state == static_cast<int>(Extension::KILLBIT)) {
      StringToLowerASCII(&key_name);
      killed_ids->insert(WideToASCII(key_name));
    }
  }
}

ExtensionPrefs::URLList ExtensionPrefs::GetShelfToolstripOrder() {
  URLList urls;
  const ListValue* toolstrip_urls = prefs_->GetList(kExtensionShelf);
  if (toolstrip_urls) {
    for (size_t i = 0; i < toolstrip_urls->GetSize(); ++i) {
      std::string url;
      if (toolstrip_urls->GetString(i, &url))
        urls.push_back(GURL(url));
    }
  }
  return urls;
}

void ExtensionPrefs::SetShelfToolstripOrder(const URLList& urls) {
  ListValue* toolstrip_urls = prefs_->GetMutableList(kExtensionShelf);
  toolstrip_urls->Clear();
  for (size_t i = 0; i < urls.size(); ++i) {
    GURL url = urls[i];
    toolstrip_urls->Append(new StringValue(url.spec()));
  }
  prefs_->ScheduleSavePersistentPrefs();
}

std::vector<std::string> ExtensionPrefs::GetToolbarOrder() {
  std::vector<std::string> extension_ids;
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
  ListValue* toolbar_order = prefs_->GetMutableList(kExtensionToolbar);
  toolbar_order->Clear();
  for (std::vector<std::string>::const_iterator iter = extension_ids.begin();
       iter != extension_ids.end(); ++iter) {
    toolbar_order->Append(new StringValue(*iter));
  }
  prefs_->ScheduleSavePersistentPrefs();
}

void ExtensionPrefs::OnExtensionInstalled(
    Extension* extension, Extension::State initial_state,
    bool initial_incognito_enabled) {
  const std::string& id = extension->id();
  UpdateExtensionPref(id, kPrefState,
                      Value::CreateIntegerValue(initial_state));
  UpdateExtensionPref(id, kPrefIncognitoEnabled,
                      Value::CreateBooleanValue(initial_incognito_enabled));
  UpdateExtensionPref(id, kPrefLocation,
                      Value::CreateIntegerValue(extension->location()));
  FilePath::StringType path = MakePathRelative(install_directory_,
      extension->path(), NULL);
  UpdateExtensionPref(id, kPrefPath, Value::CreateStringValue(path));
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (extension->location() != Extension::LOAD) {
    UpdateExtensionPref(id, kPrefManifest,
                        extension->manifest_value()->DeepCopy());
  }
  prefs_->SavePersistentPrefs();
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
                        Value::CreateIntegerValue(Extension::KILLBIT));
    prefs_->ScheduleSavePersistentPrefs();
  } else {
    DeleteExtensionPrefs(extension_id);
  }
}

Extension::State ExtensionPrefs::GetExtensionState(
    const std::string& extension_id) {
  DictionaryValue* extension = GetExtensionPref(extension_id);

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

void ExtensionPrefs::SetExtensionState(Extension* extension,
                                       Extension::State state) {
  UpdateExtensionPref(extension->id(), kPrefState,
                      Value::CreateIntegerValue(state));
  prefs_->SavePersistentPrefs();
}

std::string ExtensionPrefs::GetVersionString(const std::string& extension_id) {
  DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return std::string();

  std::string version;
  if (!extension->GetString(kPrefVersion, &version)) {
    LOG(ERROR) << "Bad or missing pref 'version' for extension '"
               << extension_id << "'";
  }

  return version;
}

void ExtensionPrefs::UpdateManifest(Extension* extension) {
  if (extension->location() != Extension::LOAD) {
    UpdateExtensionPref(extension->id(), kPrefManifest,
                        extension->manifest_value()->DeepCopy());
    prefs_->ScheduleSavePersistentPrefs();
  }
}

FilePath ExtensionPrefs::GetExtensionPath(const std::string& extension_id) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict || dict->empty())
    return FilePath();

  std::wstring path;
  if (!dict->GetString(ASCIIToWide(extension_id) + L"." + kPrefPath, &path))
    return FilePath();

  return install_directory_.Append(FilePath::FromWStringHack(path));
}

void ExtensionPrefs::UpdateExtensionPref(const std::string& extension_id,
                                         const std::wstring& key,
                                         Value* data_value) {
  if (!Extension::IdIsValid(extension_id)) {
    NOTREACHED() << "Invalid extension_id " << extension_id;
    return;
  }
  DictionaryValue* extension = GetOrCreateExtensionPref(extension_id);
  extension->Set(key, data_value);
}

void ExtensionPrefs::DeleteExtensionPrefs(const std::string& extension_id) {
  std::wstring id = ASCIIToWide(extension_id);
  DictionaryValue* dict = prefs_->GetMutableDictionary(kExtensionsPref);
  if (dict->HasKey(id)) {
    dict->Remove(id, NULL);
    prefs_->ScheduleSavePersistentPrefs();
  }
}

DictionaryValue* ExtensionPrefs::GetOrCreateExtensionPref(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  DictionaryValue* dict = prefs_->GetMutableDictionary(kExtensionsPref);
  DictionaryValue* extension = NULL;
  std::wstring id = ASCIIToWide(extension_id);
  if (!dict->GetDictionary(id, &extension)) {
    // Extension pref does not exist, create it.
    extension = new DictionaryValue();
    dict->Set(id, extension);
  }
  return extension;
}

DictionaryValue* ExtensionPrefs::GetExtensionPref(
    const std::string& extension_id) const {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict)
    return NULL;
  DictionaryValue* extension = NULL;
  std::wstring id = ASCIIToWide(extension_id);
  dict->GetDictionary(id, &extension);
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
  if (state_value == Extension::KILLBIT) {
    LOG(WARNING) << "External extension has been uninstalled by the user "
                 << *extension_id;
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

  return new ExtensionInfo(manifest, WideToASCII(*extension_id),
                           FilePath(path), location);
}

ExtensionPrefs::ExtensionsInfo* ExtensionPrefs::GetInstalledExtensionsInfo() {
  scoped_ptr<DictionaryValue> extension_data(CopyCurrentExtensions());

  ExtensionsInfo* extensions_info = new ExtensionsInfo;

  for (DictionaryValue::key_iterator extension_id(
           extension_data->begin_keys());
       extension_id != extension_data->end_keys(); ++extension_id) {
    if (!Extension::IdIsValid(WideToASCII(*extension_id)))
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
    if (WideToASCII(*extension_iter) == extension_id) {
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
  DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs) {
    NOTREACHED();
    return;
  }
  extension_prefs->Remove(kIdleInstallInfo, NULL);
  DictionaryValue* info = new DictionaryValue();
  info->SetString(kIdleInstallInfoCrxPath, crx_path.value());
  info->SetString(kIdleInstallInfoVersion, version);
  info->SetString(kIdleInstallInfoFetchTime,
                  Int64ToString(fetch_time.ToInternalValue()));
  extension_prefs->Set(kIdleInstallInfo, info);
  prefs_->ScheduleSavePersistentPrefs();
}

bool ExtensionPrefs::RemoveIdleInstallInfo(const std::string& extension_id) {
  DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return false;
  bool result = extension_prefs->Remove(kIdleInstallInfo, NULL);
  prefs_->ScheduleSavePersistentPrefs();
  return result;
}

bool ExtensionPrefs::GetIdleInstallInfo(const std::string& extension_id,
                                        FilePath* crx_path,
                                        std::string* version,
                                        base::Time* fetch_time) {
  DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
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
  if (!StringToInt64(fetch_time_string, &fetch_time_value))
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
    std::string id = WideToASCII(*iter);
    if (!Extension::IdIsValid(id)) {
      NOTREACHED();
      continue;
    }

    DictionaryValue* extension_prefs = GetExtensionPref(id);
    if (!extension_prefs)
      continue;

    DictionaryValue* info = NULL;
    if (extension_prefs->GetDictionary(kIdleInstallInfo, &info))
      result.insert(id);
  }
  return result;
}


// static
void ExtensionPrefs::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(kExtensionsPref);
  prefs->RegisterListPref(kExtensionShelf);
  prefs->RegisterListPref(kExtensionToolbar);
  prefs->RegisterIntegerPref(prefs::kExtensionToolbarSize, -1);
  prefs->RegisterDictionaryPref(kExtensionsBlacklistUpdate);
}
