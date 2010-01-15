// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs.h"

#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"

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
const wchar_t kShowInstallWarning[] = L"install_warning_on_enable";

// A preference that tracks extension shelf configuration.  This is a list
// object read from the Preferences file, containing a list of toolstrip URLs.
const wchar_t kExtensionShelf[] = L"extensions.shelf";

// A preference that tracks browser action toolbar configuration. This is a list
// object stored in the Preferences file. The extensions are stored by ID.
const wchar_t kExtensionToolbar[] = L"extensions.toolbar";

}

////////////////////////////////////////////////////////////////////////////////

ExtensionPrefs::ExtensionPrefs(PrefService* prefs, const FilePath& root_dir)
    : prefs_(prefs),
      install_directory_(root_dir) {
  if (!prefs_->FindPreference(kExtensionsPref))
    prefs_->RegisterDictionaryPref(kExtensionsPref);
  if (!prefs->FindPreference(kExtensionShelf))
    prefs->RegisterListPref(kExtensionShelf);
  if (!prefs->FindPreference(kExtensionToolbar))
    prefs->RegisterListPref(kExtensionToolbar);
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
    FilePath::StringType path_string;
    if (!extension_dict->GetString(kPrefPath, &path_string)) {
      if (!IsBlacklistBitSet(extension_dict)) {
        // We expect the kPrefPath for non-blacklisted extensions.
        NOTREACHED();
      }
      continue;
    }
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
  return ReadExtensionPrefBoolean(extension_id, kShowInstallWarning);
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

void ExtensionPrefs::OnExtensionInstalled(Extension* extension) {
  const std::string& id = extension->id();
  // Make sure we don't enable a disabled extension.
  if (GetExtensionState(extension->id()) != Extension::DISABLED) {
    UpdateExtensionPref(id, kPrefState,
                        Value::CreateIntegerValue(Extension::ENABLED));
  }
  UpdateExtensionPref(id, kPrefLocation,
                      Value::CreateIntegerValue(extension->location()));
  FilePath::StringType path = MakePathRelative(install_directory_,
      extension->path(), NULL);
  UpdateExtensionPref(id, kPrefPath, Value::CreateStringValue(path));
  UpdateExtensionPref(id, kPrefManifest,
                      extension->manifest_value()->DeepCopy());
  prefs_->SavePersistentPrefs();
}

void ExtensionPrefs::OnExtensionUninstalled(const Extension* extension,
                                            bool external_uninstall) {
  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall &&
      Extension::IsExternalLocation(extension->location())) {
    UpdateExtensionPref(extension->id(), kPrefState,
                        Value::CreateIntegerValue(Extension::KILLBIT));
    prefs_->ScheduleSavePersistentPrefs();
  } else {
    DeleteExtensionPrefs(extension->id());
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

void ExtensionPrefs::SetShowInstallWarningOnEnable(
    Extension* extension, bool require) {
  UpdateExtensionPref(extension->id(), kShowInstallWarning,
                      Value::CreateBooleanValue(require));
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
  UpdateExtensionPref(extension->id(), kPrefManifest,
                      extension->manifest_value()->DeepCopy());
  prefs_->ScheduleSavePersistentPrefs();
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
    const std::string& extension_id) {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict)
    return NULL;
  DictionaryValue* extension = NULL;
  std::wstring id = ASCIIToWide(extension_id);
  dict->GetDictionary(id, &extension);
  return extension;
}

// static
ExtensionPrefs::ExtensionsInfo* ExtensionPrefs::CollectExtensionsInfo(
    ExtensionPrefs* prefs) {
  scoped_ptr<DictionaryValue> extension_data(
      prefs->CopyCurrentExtensions());

  ExtensionsInfo* extensions_info = new ExtensionsInfo;

  for (DictionaryValue::key_iterator extension_id(
           extension_data->begin_keys());
       extension_id != extension_data->end_keys(); ++extension_id) {
    DictionaryValue* ext;
    if (!extension_data->GetDictionaryWithoutPathExpansion(*extension_id,
                                                            &ext)) {
      LOG(WARNING) << "Invalid pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    if (ext->HasKey(kPrefBlacklist)) {
      bool is_blacklisted = false;
      if (!ext->GetBoolean(kPrefBlacklist, &is_blacklisted)) {
        NOTREACHED() << "Invalid blacklist pref:" << *extension_id;
        continue;
      }
      if (is_blacklisted) {
        LOG(WARNING) << "Blacklisted extension: " << *extension_id;
        continue;
      }
    }
    int state_value;
    if (!ext->GetInteger(kPrefState, &state_value)) {
      LOG(WARNING) << "Missing state pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    if (state_value == Extension::KILLBIT) {
      LOG(WARNING) << "External extension has been uninstalled by the user "
                   << *extension_id;
      continue;
    }
    FilePath::StringType path;
    if (!ext->GetString(kPrefPath, &path)) {
      LOG(WARNING) << "Missing path pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    int location_value;
    if (!ext->GetInteger(kPrefLocation, &location_value)) {
      LOG(WARNING) << "Missing location pref for extension " << *extension_id;
      NOTREACHED();
      continue;
    }
    DictionaryValue* manifest = NULL;
    if (!ext->GetDictionary(kPrefManifest, &manifest)) {
      LOG(WARNING) << "Missing manifest for extension " << *extension_id;
      // Just a warning for now.
    }

    Extension::Location location =
        static_cast<Extension::Location>(location_value);

    extensions_info->push_back(linked_ptr<ExtensionInfo>(new ExtensionInfo(
        manifest, WideToASCII(*extension_id), FilePath(path), location)));
  }

  return extensions_info;
}
