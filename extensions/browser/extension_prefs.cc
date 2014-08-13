// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_prefs.h"

#include <iterator>

#include "base/command_line.h"
#include "base/prefs/pref_notifier.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/value_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/admin_policy.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/user_script.h"
#include "ui/base/l10n/l10n_util.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace extensions {

namespace {

// Additional preferences keys, which are not needed by external clients.

// True if this extension is running. Note this preference stops getting updated
// during Chrome shutdown (and won't be updated on a browser crash) and so can
// be used at startup to determine whether the extension was running when Chrome
// was last terminated.
const char kPrefRunning[] = "running";

// Whether this extension had windows when it was last running.
const char kIsActive[] = "is_active";

// Where an extension was installed from. (see Manifest::Location)
const char kPrefLocation[] = "location";

// Enabled, disabled, killed, etc. (see Extension::State)
const char kPrefState[] = "state";

// The path to the current version's manifest file.
const char kPrefPath[] = "path";

// The dictionary containing the extension's manifest.
const char kPrefManifest[] = "manifest";

// The version number.
const char kPrefVersion[] = "manifest.version";

// Indicates whether an extension is blacklisted.
const char kPrefBlacklist[] = "blacklist";

// If extension is greylisted.
const char kPrefBlacklistState[] = "blacklist_state";

// The count of how many times we prompted the user to acknowledge an
// extension.
const char kPrefAcknowledgePromptCount[] = "ack_prompt_count";

// Indicates whether the user has acknowledged various types of extensions.
const char kPrefExternalAcknowledged[] = "ack_external";
const char kPrefBlacklistAcknowledged[] = "ack_blacklist";
const char kPrefWipeoutAcknowledged[] = "ack_wiped";
const char kPrefSettingsBubbleAcknowledged[] = "ack_settings_bubble";
const char kPrefNtpBubbleAcknowledged[] = "ack_ntp_bubble";
const char kPrefProxyBubbleAcknowledged[] = "ack_proxy_bubble";

// Indicates whether the external extension was installed during the first
// run of this profile.
const char kPrefExternalInstallFirstRun[] = "external_first_run";

// Indicates whether to show an install warning when the user enables.
const char kExtensionDidEscalatePermissions[] = "install_warning_on_enable";

// DO NOT USE, use kPrefDisableReasons instead.
// Indicates whether the extension was updated while it was disabled.
const char kDeprecatedPrefDisableReason[] = "disable_reason";

// A bitmask of all the reasons an extension is disabled.
const char kPrefDisableReasons[] = "disable_reasons";

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

// Path for the delayed install info dictionary preference. The actual string
// value is a legacy artifact for when delayed installs only pertained to
// updates that were waiting for idle.
const char kDelayedInstallInfo[] = "idle_install_info";

// Reason why the extension's install was delayed.
const char kDelayedInstallReason[] = "delay_install_reason";

// Path for the suggested page ordinal of a delayed extension install.
const char kPrefSuggestedPageOrdinal[] = "suggested_page_ordinal";

// A preference that, if true, will allow this extension to run in incognito
// mode.
const char kPrefIncognitoEnabled[] = "incognito";

// A preference to control whether an extension is allowed to inject script in
// pages with file URLs.
const char kPrefAllowFileAccess[] = "newAllowFileAccess";
// TODO(jstritar): As part of fixing http://crbug.com/91577, we revoked all
// extension file access by renaming the pref. We should eventually clean up
// the old flag and possibly go back to that name.
// const char kPrefAllowFileAccessOld[] = "allowFileAccess";

// A preference specifying if the user dragged the app on the NTP.
const char kPrefUserDraggedApp[] = "user_dragged_app_ntp";

// Preferences that hold which permissions the user has granted the extension.
// We explicitly keep track of these so that extensions can contain unknown
// permissions, for backwards compatibility reasons, and we can still prompt
// the user to accept them once recognized. We store the active permission
// permissions because they may differ from those defined in the manifest.
const char kPrefActivePermissions[] = "active_permissions";
const char kPrefGrantedPermissions[] = "granted_permissions";

// The preference names for PermissionSet values.
const char kPrefAPIs[] = "api";
const char kPrefManifestPermissions[] = "manifest_permissions";
const char kPrefExplicitHosts[] = "explicit_host";
const char kPrefScriptableHosts[] = "scriptable_host";

// The preference names for the old granted permissions scheme.
const char kPrefOldGrantedFullAccess[] = "granted_permissions.full";
const char kPrefOldGrantedHosts[] = "granted_permissions.host";
const char kPrefOldGrantedAPIs[] = "granted_permissions.api";

// A preference that indicates when an extension was installed.
const char kPrefInstallTime[] = "install_time";

// A preference which saves the creation flags for extensions.
const char kPrefCreationFlags[] = "creation_flags";

// A preference that indicates whether the extension was installed from the
// Chrome Web Store.
const char kPrefFromWebStore[] = "from_webstore";

// A preference that indicates whether the extension was installed from a
// mock App created from a bookmark.
const char kPrefFromBookmark[] = "from_bookmark";

// A preference that indicates whether the extension was installed as a
// default app.
const char kPrefWasInstalledByDefault[] = "was_installed_by_default";

// A preference that indicates whether the extension was installed as an
// OEM app.
const char kPrefWasInstalledByOem[] = "was_installed_by_oem";

// Key for Geometry Cache preference.
const char kPrefGeometryCache[] = "geometry_cache";

// A preference that indicates when an extension is last launched.
const char kPrefLastLaunchTime[] = "last_launch_time";

// A preference indicating whether the extension is an ephemeral app.
const char kPrefEphemeralApp[] = "ephemeral_app";

// Am installation parameter bundled with an extension.
const char kPrefInstallParam[] = "install_parameter";

// A list of installed ids and a signature.
const char kInstallSignature[] = "extensions.install_signature";

// A boolean preference that indicates whether the extension should not be
// synced. Default value is false.
const char kPrefDoNotSync[] = "do_not_sync";

const char kCorruptedDisableCount[] = "extensions.corrupted_disable_count";

// Provider of write access to a dictionary storing extension prefs.
class ScopedExtensionPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedExtensionPrefUpdate(PrefService* service,
                            const std::string& extension_id) :
    DictionaryPrefUpdate(service, pref_names::kExtensions),
    extension_id_(extension_id) {}

  virtual ~ScopedExtensionPrefUpdate() {
  }

  // DictionaryPrefUpdate overrides:
  virtual base::DictionaryValue* Get() OVERRIDE {
    base::DictionaryValue* dict = DictionaryPrefUpdate::Get();
    base::DictionaryValue* extension = NULL;
    if (!dict->GetDictionary(extension_id_, &extension)) {
      // Extension pref does not exist, create it.
      extension = new base::DictionaryValue();
      dict->SetWithoutPathExpansion(extension_id_, extension);
    }
    return extension;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionPrefUpdate);
};

std::string JoinPrefs(const std::string& parent, const char* child) {
  return parent + "." + child;
}

// Checks if kPrefBlacklist is set to true in the base::DictionaryValue.
// Return false if the value is false or kPrefBlacklist does not exist.
// This is used to decide if an extension is blacklisted.
bool IsBlacklistBitSet(const base::DictionaryValue* ext) {
  bool bool_value;
  return ext->GetBoolean(kPrefBlacklist, &bool_value) && bool_value;
}

void LoadExtensionControlledPrefs(ExtensionPrefs* prefs,
                                  ExtensionPrefValueMap* value_map,
                                  const std::string& extension_id,
                                  ExtensionPrefsScope scope) {
  std::string scope_string;
  if (!pref_names::ScopeToPrefName(scope, &scope_string))
    return;
  std::string key = extension_id + "." + scope_string;

  const base::DictionaryValue* source_dict =
      prefs->pref_service()->GetDictionary(pref_names::kExtensions);
  const base::DictionaryValue* preferences = NULL;
  if (!source_dict->GetDictionary(key, &preferences))
    return;

  for (base::DictionaryValue::Iterator iter(*preferences); !iter.IsAtEnd();
       iter.Advance()) {
    value_map->SetExtensionPref(
        extension_id, iter.key(), scope, iter.value().DeepCopy());
  }
}

}  // namespace

//
// TimeProvider
//

ExtensionPrefs::TimeProvider::TimeProvider() {
}

ExtensionPrefs::TimeProvider::~TimeProvider() {
}

base::Time ExtensionPrefs::TimeProvider::GetCurrentTime() const {
  return base::Time::Now();
}

//
// ScopedUpdate
//
template <typename T, base::Value::Type type_enum_value>
ExtensionPrefs::ScopedUpdate<T, type_enum_value>::ScopedUpdate(
    ExtensionPrefs* prefs,
    const std::string& extension_id,
    const std::string& key)
    : update_(prefs->pref_service(), pref_names::kExtensions),
      extension_id_(extension_id),
      key_(key) {
  DCHECK(Extension::IdIsValid(extension_id_));
}

template <typename T, base::Value::Type type_enum_value>
ExtensionPrefs::ScopedUpdate<T, type_enum_value>::~ScopedUpdate() {
}

template <typename T, base::Value::Type type_enum_value>
T* ExtensionPrefs::ScopedUpdate<T, type_enum_value>::Get() {
  base::DictionaryValue* dict = update_.Get();
  base::DictionaryValue* extension = NULL;
  base::Value* key_value = NULL;
  if (!dict->GetDictionary(extension_id_, &extension) ||
      !extension->Get(key_, &key_value)) {
    return NULL;
  }
  return key_value->GetType() == type_enum_value ?
      static_cast<T*>(key_value) :
      NULL;
}

template <typename T, base::Value::Type type_enum_value>
T* ExtensionPrefs::ScopedUpdate<T, type_enum_value>::Create() {
  base::DictionaryValue* dict = update_.Get();
  base::DictionaryValue* extension = NULL;
  base::Value* key_value = NULL;
  T* value_as_t = NULL;
  if (!dict->GetDictionary(extension_id_, &extension)) {
    extension = new base::DictionaryValue;
    dict->SetWithoutPathExpansion(extension_id_, extension);
  }
  if (!extension->Get(key_, &key_value)) {
    value_as_t = new T;
    extension->SetWithoutPathExpansion(key_, value_as_t);
  } else {
    CHECK(key_value->GetType() == type_enum_value);
    value_as_t = static_cast<T*>(key_value);
  }
  return value_as_t;
}

// Explicit instantiations for Dictionary and List value types.
template class ExtensionPrefs::ScopedUpdate<base::DictionaryValue,
                                            base::Value::TYPE_DICTIONARY>;
template class ExtensionPrefs::ScopedUpdate<base::ListValue,
                                            base::Value::TYPE_LIST>;

//
// ExtensionPrefs
//

// static
ExtensionPrefs* ExtensionPrefs::Create(
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    scoped_ptr<AppSorting> app_sorting,
    bool extensions_disabled,
    const std::vector<ExtensionPrefsObserver*>& early_observers) {
  return ExtensionPrefs::Create(prefs,
                                root_dir,
                                extension_pref_value_map,
                                app_sorting.Pass(),
                                extensions_disabled,
                                early_observers,
                                make_scoped_ptr(new TimeProvider()));
}

// static
ExtensionPrefs* ExtensionPrefs::Create(
    PrefService* pref_service,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    scoped_ptr<AppSorting> app_sorting,
    bool extensions_disabled,
    const std::vector<ExtensionPrefsObserver*>& early_observers,
    scoped_ptr<TimeProvider> time_provider) {
  return new ExtensionPrefs(pref_service,
                            root_dir,
                            extension_pref_value_map,
                            app_sorting.Pass(),
                            time_provider.Pass(),
                            extensions_disabled,
                            early_observers);
}

ExtensionPrefs::~ExtensionPrefs() {
}

// static
ExtensionPrefs* ExtensionPrefs::Get(content::BrowserContext* context) {
  return ExtensionPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

static base::FilePath::StringType MakePathRelative(const base::FilePath& parent,
                                             const base::FilePath& child) {
  if (!parent.IsParent(child))
    return child.value();

  base::FilePath::StringType retval = child.value().substr(
      parent.value().length());
  if (base::FilePath::IsSeparator(retval[0]))
    return retval.substr(1);
  else
    return retval;
}

void ExtensionPrefs::MakePathsRelative() {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!dict || dict->empty())
    return;

  // Collect all extensions ids with absolute paths in |absolute_keys|.
  std::set<std::string> absolute_keys;
  for (base::DictionaryValue::Iterator i(*dict); !i.IsAtEnd(); i.Advance()) {
    const base::DictionaryValue* extension_dict = NULL;
    if (!i.value().GetAsDictionary(&extension_dict))
      continue;
    int location_value;
    if (extension_dict->GetInteger(kPrefLocation, &location_value) &&
        Manifest::IsUnpackedLocation(
            static_cast<Manifest::Location>(location_value))) {
      // Unpacked extensions can have absolute paths.
      continue;
    }
    base::FilePath::StringType path_string;
    if (!extension_dict->GetString(kPrefPath, &path_string))
      continue;
    base::FilePath path(path_string);
    if (path.IsAbsolute())
      absolute_keys.insert(i.key());
  }
  if (absolute_keys.empty())
    return;

  // Fix these paths.
  DictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  base::DictionaryValue* update_dict = update.Get();
  for (std::set<std::string>::iterator i = absolute_keys.begin();
       i != absolute_keys.end(); ++i) {
    base::DictionaryValue* extension_dict = NULL;
    if (!update_dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict)) {
      NOTREACHED() << "Control should never reach here for extension " << *i;
      continue;
    }
    base::FilePath::StringType path_string;
    extension_dict->GetString(kPrefPath, &path_string);
    base::FilePath path(path_string);
    extension_dict->SetString(kPrefPath,
        MakePathRelative(install_directory_, path));
  }
}

const base::DictionaryValue* ExtensionPrefs::GetExtensionPref(
    const std::string& extension_id) const {
  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  const base::DictionaryValue* extension_dict = NULL;
  if (!extensions ||
      !extensions->GetDictionary(extension_id, &extension_dict)) {
    return NULL;
  }
  return extension_dict;
}

void ExtensionPrefs::UpdateExtensionPref(const std::string& extension_id,
                                         const std::string& key,
                                         base::Value* data_value) {
  if (!Extension::IdIsValid(extension_id)) {
    NOTREACHED() << "Invalid extension_id " << extension_id;
    return;
  }
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  if (data_value)
    update->Set(key, data_value);
  else
    update->Remove(key, NULL);
}

void ExtensionPrefs::DeleteExtensionPrefs(const std::string& extension_id) {
  extension_pref_value_map_->UnregisterExtension(extension_id);
  FOR_EACH_OBSERVER(ExtensionPrefsObserver,
                    observer_list_,
                    OnExtensionPrefsDeleted(extension_id));
  DictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  base::DictionaryValue* dict = update.Get();
  dict->Remove(extension_id, NULL);
}

bool ExtensionPrefs::ReadPrefAsBoolean(const std::string& extension_id,
                                       const std::string& pref_key,
                                       bool* out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetBoolean(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadPrefAsInteger(const std::string& extension_id,
                                       const std::string& pref_key,
                                       int* out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetInteger(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadPrefAsString(const std::string& extension_id,
                                      const std::string& pref_key,
                                      std::string* out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetString(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadPrefAsList(const std::string& extension_id,
                                    const std::string& pref_key,
                                    const base::ListValue** out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  const base::ListValue* out = NULL;
  if (!ext || !ext->GetList(pref_key, &out))
    return false;
  if (out_value)
    *out_value = out;

  return true;
}

bool ExtensionPrefs::ReadPrefAsDictionary(
    const std::string& extension_id,
    const std::string& pref_key,
    const base::DictionaryValue** out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  const base::DictionaryValue* out = NULL;
  if (!ext || !ext->GetDictionary(pref_key, &out))
    return false;
  if (out_value)
    *out_value = out;

  return true;
}

bool ExtensionPrefs::HasPrefForExtension(
    const std::string& extension_id) const {
  return GetExtensionPref(extension_id) != NULL;
}

bool ExtensionPrefs::ReadPrefAsURLPatternSet(const std::string& extension_id,
                                             const std::string& pref_key,
                                             URLPatternSet* result,
                                             int valid_schemes) {
  const base::ListValue* value = NULL;
  if (!ReadPrefAsList(extension_id, pref_key, &value))
    return false;

  bool allow_file_access = AllowFileAccess(extension_id);
  return result->Populate(*value, valid_schemes, allow_file_access, NULL);
}

void ExtensionPrefs::SetExtensionPrefURLPatternSet(
    const std::string& extension_id,
    const std::string& pref_key,
    const URLPatternSet& new_value) {
  UpdateExtensionPref(extension_id, pref_key, new_value.ToValue().release());
}

bool ExtensionPrefs::ReadPrefAsBooleanAndReturn(
    const std::string& extension_id,
    const std::string& pref_key) const {
  bool out_value = false;
  return ReadPrefAsBoolean(extension_id, pref_key, &out_value) && out_value;
}

PermissionSet* ExtensionPrefs::ReadPrefAsPermissionSet(
    const std::string& extension_id,
    const std::string& pref_key) {
  if (!GetExtensionPref(extension_id))
    return NULL;

  // Retrieve the API permissions. Please refer SetExtensionPrefPermissionSet()
  // for api_values format.
  APIPermissionSet apis;
  const base::ListValue* api_values = NULL;
  std::string api_pref = JoinPrefs(pref_key, kPrefAPIs);
  if (ReadPrefAsList(extension_id, api_pref, &api_values)) {
    APIPermissionSet::ParseFromJSON(api_values,
                                    APIPermissionSet::kAllowInternalPermissions,
                                    &apis, NULL, NULL);
  }

  // Retrieve the Manifest Keys permissions. Please refer to
  // |SetExtensionPrefPermissionSet| for manifest_permissions_values format.
  ManifestPermissionSet manifest_permissions;
  const base::ListValue* manifest_permissions_values = NULL;
  std::string manifest_permission_pref =
      JoinPrefs(pref_key, kPrefManifestPermissions);
  if (ReadPrefAsList(extension_id, manifest_permission_pref,
                     &manifest_permissions_values)) {
    ManifestPermissionSet::ParseFromJSON(
        manifest_permissions_values, &manifest_permissions, NULL, NULL);
  }

  // Retrieve the explicit host permissions.
  URLPatternSet explicit_hosts;
  ReadPrefAsURLPatternSet(
      extension_id, JoinPrefs(pref_key, kPrefExplicitHosts),
      &explicit_hosts, Extension::kValidHostPermissionSchemes);

  // Retrieve the scriptable host permissions.
  URLPatternSet scriptable_hosts;
  ReadPrefAsURLPatternSet(
      extension_id, JoinPrefs(pref_key, kPrefScriptableHosts),
      &scriptable_hosts, UserScript::ValidUserScriptSchemes());

  return new PermissionSet(
      apis, manifest_permissions, explicit_hosts, scriptable_hosts);
}

// Set the API or Manifest permissions.
// The format of api_values is:
// [ "permission_name1",   // permissions do not support detail.
//   "permission_name2",
//   {"permission_name3": value },
//   // permission supports detail, permission detail will be stored in value.
//   ...
// ]
template<typename T>
static base::ListValue* CreatePermissionList(const T& permissions) {
  base::ListValue* values = new base::ListValue();
  for (typename T::const_iterator i = permissions.begin();
      i != permissions.end(); ++i) {
    scoped_ptr<base::Value> detail(i->ToValue());
    if (detail) {
      base::DictionaryValue* tmp = new base::DictionaryValue();
      tmp->Set(i->name(), detail.release());
      values->Append(tmp);
    } else {
      values->Append(new base::StringValue(i->name()));
    }
  }
  return values;
}

void ExtensionPrefs::SetExtensionPrefPermissionSet(
    const std::string& extension_id,
    const std::string& pref_key,
    const PermissionSet* new_value) {
  std::string api_pref = JoinPrefs(pref_key, kPrefAPIs);
  base::ListValue* api_values = CreatePermissionList(new_value->apis());
  UpdateExtensionPref(extension_id, api_pref, api_values);

  std::string manifest_permissions_pref =
      JoinPrefs(pref_key, kPrefManifestPermissions);
  base::ListValue* manifest_permissions_values = CreatePermissionList(
      new_value->manifest_permissions());
  UpdateExtensionPref(extension_id,
                      manifest_permissions_pref,
                      manifest_permissions_values);

  // Set the explicit host permissions.
  if (!new_value->explicit_hosts().is_empty()) {
    SetExtensionPrefURLPatternSet(extension_id,
                                  JoinPrefs(pref_key, kPrefExplicitHosts),
                                  new_value->explicit_hosts());
  }

  // Set the scriptable host permissions.
  if (!new_value->scriptable_hosts().is_empty()) {
    SetExtensionPrefURLPatternSet(extension_id,
                                  JoinPrefs(pref_key, kPrefScriptableHosts),
                                  new_value->scriptable_hosts());
  }
}

int ExtensionPrefs::IncrementAcknowledgePromptCount(
    const std::string& extension_id) {
  int count = 0;
  ReadPrefAsInteger(extension_id, kPrefAcknowledgePromptCount, &count);
  ++count;
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount,
                      new base::FundamentalValue(count));
  return count;
}

bool ExtensionPrefs::IsExternalExtensionAcknowledged(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefExternalAcknowledged);
}

void ExtensionPrefs::AcknowledgeExternalExtension(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalAcknowledged,
                      new base::FundamentalValue(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, NULL);
}

bool ExtensionPrefs::IsBlacklistedExtensionAcknowledged(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefBlacklistAcknowledged);
}

void ExtensionPrefs::AcknowledgeBlacklistedExtension(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefBlacklistAcknowledged,
                      new base::FundamentalValue(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, NULL);
}

bool ExtensionPrefs::IsExternalInstallFirstRun(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefExternalInstallFirstRun);
}

void ExtensionPrefs::SetExternalInstallFirstRun(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalInstallFirstRun,
                      new base::FundamentalValue(true));
}

bool ExtensionPrefs::HasWipeoutBeenAcknowledged(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefWipeoutAcknowledged);
}

void ExtensionPrefs::SetWipeoutAcknowledged(
    const std::string& extension_id,
    bool value) {
  UpdateExtensionPref(extension_id,
                      kPrefWipeoutAcknowledged,
                      value ? new base::FundamentalValue(value) : NULL);
}

bool ExtensionPrefs::HasSettingsApiBubbleBeenAcknowledged(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id,
                                    kPrefSettingsBubbleAcknowledged);
}

void ExtensionPrefs::SetSettingsApiBubbleBeenAcknowledged(
    const std::string& extension_id,
    bool value) {
  UpdateExtensionPref(extension_id,
                      kPrefSettingsBubbleAcknowledged,
                      value ? new base::FundamentalValue(value) : NULL);
}

bool ExtensionPrefs::HasNtpOverriddenBubbleBeenAcknowledged(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefNtpBubbleAcknowledged);
}

void ExtensionPrefs::SetNtpOverriddenBubbleBeenAcknowledged(
    const std::string& extension_id,
    bool value) {
  UpdateExtensionPref(extension_id,
                      kPrefNtpBubbleAcknowledged,
                      value ? new base::FundamentalValue(value) : NULL);
}

bool ExtensionPrefs::HasProxyOverriddenBubbleBeenAcknowledged(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefProxyBubbleAcknowledged);
}

void ExtensionPrefs::SetProxyOverriddenBubbleBeenAcknowledged(
    const std::string& extension_id,
    bool value) {
  UpdateExtensionPref(extension_id,
                      kPrefProxyBubbleAcknowledged,
                      value ? new base::FundamentalValue(value) : NULL);
}

bool ExtensionPrefs::SetAlertSystemFirstRun() {
  if (prefs_->GetBoolean(pref_names::kAlertsInitialized)) {
    return true;
  }
  prefs_->SetBoolean(pref_names::kAlertsInitialized, true);
  return false;
}

bool ExtensionPrefs::ExtensionsBlacklistedByDefault() const {
  return admin_policy::BlacklistedByDefault(
      prefs_->GetList(pref_names::kInstallDenyList));
}

bool ExtensionPrefs::DidExtensionEscalatePermissions(
    const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id,
                                    kExtensionDidEscalatePermissions);
}

void ExtensionPrefs::SetDidExtensionEscalatePermissions(
    const Extension* extension, bool did_escalate) {
  UpdateExtensionPref(extension->id(), kExtensionDidEscalatePermissions,
                      new base::FundamentalValue(did_escalate));
}

int ExtensionPrefs::GetDisableReasons(const std::string& extension_id) const {
  int value = -1;
  if (ReadPrefAsInteger(extension_id, kPrefDisableReasons, &value) &&
      value >= 0) {
    return value;
  }
  return Extension::DISABLE_NONE;
}

bool ExtensionPrefs::HasDisableReason(
    const std::string& extension_id,
    Extension::DisableReason disable_reason) const {
  return (GetDisableReasons(extension_id) & disable_reason) != 0;
}

void ExtensionPrefs::AddDisableReason(const std::string& extension_id,
                                      Extension::DisableReason disable_reason) {
  ModifyDisableReason(extension_id, disable_reason, DISABLE_REASON_ADD);
}

void ExtensionPrefs::RemoveDisableReason(
    const std::string& extension_id,
    Extension::DisableReason disable_reason) {
  ModifyDisableReason(extension_id, disable_reason, DISABLE_REASON_REMOVE);
}

void ExtensionPrefs::ClearDisableReasons(const std::string& extension_id) {
  ModifyDisableReason(
      extension_id, Extension::DISABLE_NONE, DISABLE_REASON_CLEAR);
}

void ExtensionPrefs::ModifyDisableReason(const std::string& extension_id,
                                         Extension::DisableReason reason,
                                         DisableReasonChange change) {
  int old_value = GetDisableReasons(extension_id);
  int new_value = old_value;
  switch (change) {
    case DISABLE_REASON_ADD:
      new_value |= static_cast<int>(reason);
      break;
    case DISABLE_REASON_REMOVE:
      new_value &= ~static_cast<int>(reason);
      break;
    case DISABLE_REASON_CLEAR:
      new_value = Extension::DISABLE_NONE;
      break;
  }

  if (old_value == new_value)  // no change, return.
    return;

  if (new_value == Extension::DISABLE_NONE) {
    UpdateExtensionPref(extension_id, kPrefDisableReasons, NULL);
  } else {
    UpdateExtensionPref(extension_id,
                        kPrefDisableReasons,
                        new base::FundamentalValue(new_value));
  }

  FOR_EACH_OBSERVER(ExtensionPrefsObserver,
                    observer_list_,
                    OnExtensionDisableReasonsChanged(extension_id, new_value));
}

std::set<std::string> ExtensionPrefs::GetBlacklistedExtensions() {
  std::set<std::string> ids;

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!extensions)
    return ids;

  for (base::DictionaryValue::Iterator it(*extensions);
       !it.IsAtEnd(); it.Advance()) {
    if (!it.value().IsType(base::Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Invalid pref for extension " << it.key();
      continue;
    }
    if (IsBlacklistBitSet(
            static_cast<const base::DictionaryValue*>(&it.value()))) {
      ids.insert(it.key());
    }
  }

  return ids;
}

void ExtensionPrefs::SetExtensionBlacklisted(const std::string& extension_id,
                                             bool is_blacklisted) {
  bool currently_blacklisted = IsExtensionBlacklisted(extension_id);
  if (is_blacklisted == currently_blacklisted)
    return;

  // Always make sure the "acknowledged" bit is cleared since the blacklist bit
  // is changing.
  UpdateExtensionPref(extension_id, kPrefBlacklistAcknowledged, NULL);

  if (is_blacklisted) {
    UpdateExtensionPref(extension_id,
                        kPrefBlacklist,
                        new base::FundamentalValue(true));
  } else {
    UpdateExtensionPref(extension_id, kPrefBlacklist, NULL);
    const base::DictionaryValue* dict = GetExtensionPref(extension_id);
    if (dict && dict->empty())
      DeleteExtensionPrefs(extension_id);
  }
}

bool ExtensionPrefs::IsExtensionBlacklisted(const std::string& id) const {
  const base::DictionaryValue* ext_prefs = GetExtensionPref(id);
  return ext_prefs && IsBlacklistBitSet(ext_prefs);
}

namespace {

// Serializes a 64bit integer as a string value.
void SaveInt64(base::DictionaryValue* dictionary,
               const char* key,
               const int64 value) {
  if (!dictionary)
    return;

  std::string string_value = base::Int64ToString(value);
  dictionary->SetString(key, string_value);
}

// Deserializes a 64bit integer stored as a string value.
bool ReadInt64(const base::DictionaryValue* dictionary,
               const char* key,
               int64* value) {
  if (!dictionary)
    return false;

  std::string string_value;
  if (!dictionary->GetString(key, &string_value))
    return false;

  return base::StringToInt64(string_value, value);
}

// Serializes |time| as a string value mapped to |key| in |dictionary|.
void SaveTime(base::DictionaryValue* dictionary,
              const char* key,
              const base::Time& time) {
  SaveInt64(dictionary, key, time.ToInternalValue());
}

// The opposite of SaveTime. If |key| is not found, this returns an empty Time
// (is_null() will return true).
base::Time ReadTime(const base::DictionaryValue* dictionary, const char* key) {
  int64 value;
  if (ReadInt64(dictionary, key, &value))
    return base::Time::FromInternalValue(value);

  return base::Time();
}

}  // namespace

base::Time ExtensionPrefs::LastPingDay(const std::string& extension_id) const {
  DCHECK(Extension::IdIsValid(extension_id));
  return ReadTime(GetExtensionPref(extension_id), kLastPingDay);
}

void ExtensionPrefs::SetLastPingDay(const std::string& extension_id,
                                    const base::Time& time) {
  DCHECK(Extension::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get(), kLastPingDay, time);
}

base::Time ExtensionPrefs::BlacklistLastPingDay() const {
  return ReadTime(prefs_->GetDictionary(kExtensionsBlacklistUpdate),
                  kLastPingDay);
}

void ExtensionPrefs::SetBlacklistLastPingDay(const base::Time& time) {
  DictionaryPrefUpdate update(prefs_, kExtensionsBlacklistUpdate);
  SaveTime(update.Get(), kLastPingDay, time);
}

base::Time ExtensionPrefs::LastActivePingDay(const std::string& extension_id) {
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
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kActiveBit, &result))
    return result;
  return false;
}

void ExtensionPrefs::SetActiveBit(const std::string& extension_id,
                                  bool active) {
  UpdateExtensionPref(extension_id, kActiveBit,
                      new base::FundamentalValue(active));
}

void ExtensionPrefs::MigratePermissions(const ExtensionIdList& extension_ids) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  for (ExtensionIdList::const_iterator ext_id =
       extension_ids.begin(); ext_id != extension_ids.end(); ++ext_id) {
    // An extension's granted permissions need to be migrated if the
    // full_access bit is present. This bit was always present in the previous
    // scheme and is never present now.
    bool full_access = false;
    const base::DictionaryValue* ext = GetExtensionPref(*ext_id);
    if (!ext || !ext->GetBoolean(kPrefOldGrantedFullAccess, &full_access))
      continue;

    // Remove the full access bit (empty list will get trimmed).
    UpdateExtensionPref(
        *ext_id, kPrefOldGrantedFullAccess, new base::ListValue());

    // Add the plugin permission if the full access bit was set.
    if (full_access) {
      const base::ListValue* apis = NULL;
      base::ListValue* new_apis = NULL;

      std::string granted_apis = JoinPrefs(kPrefGrantedPermissions, kPrefAPIs);
      if (ext->GetList(kPrefOldGrantedAPIs, &apis))
        new_apis = apis->DeepCopy();
      else
        new_apis = new base::ListValue();

      std::string plugin_name = info->GetByID(APIPermission::kPlugin)->name();
      new_apis->Append(new base::StringValue(plugin_name));
      UpdateExtensionPref(*ext_id, granted_apis, new_apis);
    }

    // The granted permissions originally only held the effective hosts,
    // which are a combination of host and user script host permissions.
    // We now maintain these lists separately. For migration purposes, it
    // does not matter how we treat the old effective hosts as long as the
    // new effective hosts will be the same, so we move them to explicit
    // host permissions.
    const base::ListValue* hosts = NULL;
    std::string explicit_hosts =
        JoinPrefs(kPrefGrantedPermissions, kPrefExplicitHosts);
    if (ext->GetList(kPrefOldGrantedHosts, &hosts)) {
      UpdateExtensionPref(
          *ext_id, explicit_hosts, hosts->DeepCopy());

      // We can get rid of the old one by setting it to an empty list.
      UpdateExtensionPref(*ext_id, kPrefOldGrantedHosts, new base::ListValue());
    }
  }
}

void ExtensionPrefs::MigrateDisableReasons(
    const ExtensionIdList& extension_ids) {
  for (ExtensionIdList::const_iterator ext_id =
       extension_ids.begin(); ext_id != extension_ids.end(); ++ext_id) {
    int value = -1;
    if (ReadPrefAsInteger(*ext_id, kDeprecatedPrefDisableReason, &value)) {
      int new_value = Extension::DISABLE_NONE;
      switch (value) {
        case Extension::DEPRECATED_DISABLE_USER_ACTION:
          new_value = Extension::DISABLE_USER_ACTION;
          break;
        case Extension::DEPRECATED_DISABLE_PERMISSIONS_INCREASE:
          new_value = Extension::DISABLE_PERMISSIONS_INCREASE;
          break;
        case Extension::DEPRECATED_DISABLE_RELOAD:
          new_value = Extension::DISABLE_RELOAD;
          break;
      }

      UpdateExtensionPref(*ext_id, kPrefDisableReasons,
                          new base::FundamentalValue(new_value));
      // Remove the old disable reason.
      UpdateExtensionPref(*ext_id, kDeprecatedPrefDisableReason, NULL);
    }
  }
}

PermissionSet* ExtensionPrefs::GetGrantedPermissions(
    const std::string& extension_id) {
  CHECK(Extension::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefGrantedPermissions);
}

void ExtensionPrefs::AddGrantedPermissions(
    const std::string& extension_id,
    const PermissionSet* permissions) {
  CHECK(Extension::IdIsValid(extension_id));

  scoped_refptr<PermissionSet> granted_permissions(
      GetGrantedPermissions(extension_id));

  // The new granted permissions are the union of the already granted
  // permissions and the newly granted permissions.
  scoped_refptr<PermissionSet> new_perms(
      PermissionSet::CreateUnion(
          permissions, granted_permissions.get()));

  SetExtensionPrefPermissionSet(
      extension_id, kPrefGrantedPermissions, new_perms.get());
}

void ExtensionPrefs::RemoveGrantedPermissions(
    const std::string& extension_id,
    const PermissionSet* permissions) {
  CHECK(Extension::IdIsValid(extension_id));

  scoped_refptr<PermissionSet> granted_permissions(
      GetGrantedPermissions(extension_id));

  // The new granted permissions are the difference of the already granted
  // permissions and the newly ungranted permissions.
  scoped_refptr<PermissionSet> new_perms(
      PermissionSet::CreateDifference(
          granted_permissions.get(), permissions));

  SetExtensionPrefPermissionSet(
      extension_id, kPrefGrantedPermissions, new_perms.get());
}

PermissionSet* ExtensionPrefs::GetActivePermissions(
    const std::string& extension_id) {
  CHECK(Extension::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefActivePermissions);
}

void ExtensionPrefs::SetActivePermissions(
    const std::string& extension_id,
    const PermissionSet* permissions) {
  SetExtensionPrefPermissionSet(
      extension_id, kPrefActivePermissions, permissions);
}

void ExtensionPrefs::SetExtensionRunning(const std::string& extension_id,
    bool is_running) {
  base::Value* value = new base::FundamentalValue(is_running);
  UpdateExtensionPref(extension_id, kPrefRunning, value);
}

bool ExtensionPrefs::IsExtensionRunning(const std::string& extension_id) {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  bool running = false;
  extension->GetBoolean(kPrefRunning, &running);
  return running;
}

void ExtensionPrefs::SetIsActive(const std::string& extension_id,
                                 bool is_active) {
  base::Value* value = new base::FundamentalValue(is_active);
  UpdateExtensionPref(extension_id, kIsActive, value);
}

bool ExtensionPrefs::IsActive(const std::string& extension_id) {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  bool is_active = false;
  extension->GetBoolean(kIsActive, &is_active);
  return is_active;
}

bool ExtensionPrefs::IsIncognitoEnabled(const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefIncognitoEnabled);
}

void ExtensionPrefs::SetIsIncognitoEnabled(const std::string& extension_id,
                                           bool enabled) {
  UpdateExtensionPref(extension_id, kPrefIncognitoEnabled,
                      new base::FundamentalValue(enabled));
  extension_pref_value_map_->SetExtensionIncognitoState(extension_id, enabled);
}

bool ExtensionPrefs::AllowFileAccess(const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefAllowFileAccess);
}

void ExtensionPrefs::SetAllowFileAccess(const std::string& extension_id,
                                        bool allow) {
  UpdateExtensionPref(extension_id, kPrefAllowFileAccess,
                      new base::FundamentalValue(allow));
}

bool ExtensionPrefs::HasAllowFileAccessSetting(
    const std::string& extension_id) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  return ext && ext->HasKey(kPrefAllowFileAccess);
}

bool ExtensionPrefs::DoesExtensionHaveState(
    const std::string& id, Extension::State check_state) const {
  const base::DictionaryValue* extension = GetExtensionPref(id);
  int state = -1;
  if (!extension || !extension->GetInteger(kPrefState, &state))
    return false;

  if (state < 0 || state >= Extension::NUM_STATES) {
    LOG(ERROR) << "Bad pref 'state' for extension '" << id << "'";
    return false;
  }

  return state == check_state;
}

bool ExtensionPrefs::IsExternalExtensionUninstalled(
    const std::string& id) const {
  return DoesExtensionHaveState(id, Extension::EXTERNAL_EXTENSION_UNINSTALLED);
}

bool ExtensionPrefs::IsExtensionDisabled(
    const std::string& id) const {
  return DoesExtensionHaveState(id, Extension::DISABLED);
}

ExtensionIdList ExtensionPrefs::GetToolbarOrder() {
  ExtensionIdList id_list_out;
  GetUserExtensionPrefIntoContainer(pref_names::kToolbar, &id_list_out);
  return id_list_out;
}

void ExtensionPrefs::SetToolbarOrder(const ExtensionIdList& extension_ids) {
  SetExtensionPrefFromContainer(pref_names::kToolbar, extension_ids);
}

void ExtensionPrefs::OnExtensionInstalled(
    const Extension* extension,
    Extension::State initial_state,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    const std::string& install_parameter) {
  ScopedExtensionPrefUpdate update(prefs_, extension->id());
  base::DictionaryValue* extension_dict = update.Get();
  const base::Time install_time = time_provider_->GetCurrentTime();
  PopulateExtensionInfoPrefs(extension,
                             install_time,
                             initial_state,
                             install_flags,
                             install_parameter,
                             extension_dict);

  bool requires_sort_ordinal = extension->RequiresSortOrdinal() &&
                               (install_flags & kInstallFlagIsEphemeral) == 0;
  FinishExtensionInfoPrefs(extension->id(),
                           install_time,
                           requires_sort_ordinal,
                           page_ordinal,
                           extension_dict);
}

void ExtensionPrefs::OnExtensionUninstalled(const std::string& extension_id,
                                            const Manifest::Location& location,
                                            bool external_uninstall) {
  app_sorting_->ClearOrdinals(extension_id);

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall && Manifest::IsExternalLocation(location)) {
    UpdateExtensionPref(extension_id, kPrefState,
                        new base::FundamentalValue(
                            Extension::EXTERNAL_EXTENSION_UNINSTALLED));
    extension_pref_value_map_->SetExtensionState(extension_id, false);
    FOR_EACH_OBSERVER(ExtensionPrefsObserver,
                      observer_list_,
                      OnExtensionStateChanged(extension_id, false));
  } else {
    DeleteExtensionPrefs(extension_id);
  }
}

void ExtensionPrefs::SetExtensionState(const std::string& extension_id,
                                       Extension::State state) {
  UpdateExtensionPref(extension_id, kPrefState,
                      new base::FundamentalValue(state));
  bool enabled = (state == Extension::ENABLED);
  extension_pref_value_map_->SetExtensionState(extension_id, enabled);
  FOR_EACH_OBSERVER(ExtensionPrefsObserver,
                    observer_list_,
                    OnExtensionStateChanged(extension_id, enabled));
}

void ExtensionPrefs::SetExtensionBlacklistState(const std::string& extension_id,
                                                BlacklistState state) {
  SetExtensionBlacklisted(extension_id, state == BLACKLISTED_MALWARE);
  UpdateExtensionPref(extension_id, kPrefBlacklistState,
                      new base::FundamentalValue(state));
}

BlacklistState ExtensionPrefs::GetExtensionBlacklistState(
    const std::string& extension_id) {
  if (IsExtensionBlacklisted(extension_id))
    return BLACKLISTED_MALWARE;
  const base::DictionaryValue* ext_prefs = GetExtensionPref(extension_id);
  int int_value = 0;
  if (ext_prefs && ext_prefs->GetInteger(kPrefBlacklistState, &int_value))
    return static_cast<BlacklistState>(int_value);

  return NOT_BLACKLISTED;
}

std::string ExtensionPrefs::GetVersionString(const std::string& extension_id) {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return std::string();

  std::string version;
  extension->GetString(kPrefVersion, &version);

  return version;
}

void ExtensionPrefs::UpdateManifest(const Extension* extension) {
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    const base::DictionaryValue* extension_dict =
        GetExtensionPref(extension->id());
    if (!extension_dict)
      return;
    const base::DictionaryValue* old_manifest = NULL;
    bool update_required =
        !extension_dict->GetDictionary(kPrefManifest, &old_manifest) ||
        !extension->manifest()->value()->Equals(old_manifest);
    if (update_required) {
      UpdateExtensionPref(extension->id(), kPrefManifest,
                          extension->manifest()->value()->DeepCopy());
    }
  }
}

scoped_ptr<ExtensionInfo> ExtensionPrefs::GetInstalledInfoHelper(
    const std::string& extension_id,
    const base::DictionaryValue* extension) const {
  int location_value;
  if (!extension->GetInteger(kPrefLocation, &location_value))
    return scoped_ptr<ExtensionInfo>();

  base::FilePath::StringType path;
  if (!extension->GetString(kPrefPath, &path))
    return scoped_ptr<ExtensionInfo>();

  // Make path absolute. Unpacked extensions will already have absolute paths,
  // otherwise make it so.
  Manifest::Location location = static_cast<Manifest::Location>(location_value);
#if !defined(OS_CHROMEOS)
  if (!Manifest::IsUnpackedLocation(location)) {
    DCHECK(location == Manifest::COMPONENT ||
           !base::FilePath(path).IsAbsolute());
#else
  // On Chrome OS some extensions can be installed to shared location and
  // thus use absolute paths in prefs.
  if (!base::FilePath(path).IsAbsolute()) {
#endif  // !defined(OS_CHROMEOS)
    path = install_directory_.Append(path).value();
  }

  // Only the following extension types have data saved in the preferences.
  if (location != Manifest::INTERNAL &&
      !Manifest::IsUnpackedLocation(location) &&
      !Manifest::IsExternalLocation(location)) {
    NOTREACHED();
    return scoped_ptr<ExtensionInfo>();
  }

  const base::DictionaryValue* manifest = NULL;
  if (!Manifest::IsUnpackedLocation(location) &&
      !extension->GetDictionary(kPrefManifest, &manifest)) {
    LOG(WARNING) << "Missing manifest for extension " << extension_id;
    // Just a warning for now.
  }

  return scoped_ptr<ExtensionInfo>(new ExtensionInfo(
      manifest, extension_id, base::FilePath(path), location));
}

scoped_ptr<ExtensionInfo> ExtensionPrefs::GetInstalledExtensionInfo(
    const std::string& extension_id) const {
  const base::DictionaryValue* ext = NULL;
  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!extensions ||
      !extensions->GetDictionaryWithoutPathExpansion(extension_id, &ext))
    return scoped_ptr<ExtensionInfo>();
  int state_value;
  if (!ext->GetInteger(kPrefState, &state_value) ||
      state_value == Extension::ENABLED_COMPONENT) {
    // Old preferences files may not have kPrefState for component extensions.
    return scoped_ptr<ExtensionInfo>();
  }

  if (state_value == Extension::EXTERNAL_EXTENSION_UNINSTALLED) {
    LOG(WARNING) << "External extension with id " << extension_id
                 << " has been uninstalled by the user";
    return scoped_ptr<ExtensionInfo>();
  }

  return GetInstalledInfoHelper(extension_id, ext);
}

scoped_ptr<ExtensionPrefs::ExtensionsInfo>
ExtensionPrefs::GetInstalledExtensionsInfo() const {
  scoped_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  for (base::DictionaryValue::Iterator extension_id(*extensions);
       !extension_id.IsAtEnd(); extension_id.Advance()) {
    if (!Extension::IdIsValid(extension_id.key()))
      continue;

    scoped_ptr<ExtensionInfo> info =
        GetInstalledExtensionInfo(extension_id.key());
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info.release()));
  }

  return extensions_info.Pass();
}

scoped_ptr<ExtensionPrefs::ExtensionsInfo>
ExtensionPrefs::GetUninstalledExtensionsInfo() const {
  scoped_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  for (base::DictionaryValue::Iterator extension_id(*extensions);
       !extension_id.IsAtEnd(); extension_id.Advance()) {
    const base::DictionaryValue* ext = NULL;
    if (!Extension::IdIsValid(extension_id.key()) ||
        !IsExternalExtensionUninstalled(extension_id.key()) ||
        !extension_id.value().GetAsDictionary(&ext))
      continue;

    scoped_ptr<ExtensionInfo> info =
        GetInstalledInfoHelper(extension_id.key(), ext);
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info.release()));
  }

  return extensions_info.Pass();
}

void ExtensionPrefs::SetDelayedInstallInfo(
    const Extension* extension,
    Extension::State initial_state,
    int install_flags,
    DelayReason delay_reason,
    const syncer::StringOrdinal& page_ordinal,
    const std::string& install_parameter) {
  base::DictionaryValue* extension_dict = new base::DictionaryValue();
  PopulateExtensionInfoPrefs(extension,
                             time_provider_->GetCurrentTime(),
                             initial_state,
                             install_flags,
                             install_parameter,
                             extension_dict);

  // Add transient data that is needed by FinishDelayedInstallInfo(), but
  // should not be in the final extension prefs. All entries here should have
  // a corresponding Remove() call in FinishDelayedInstallInfo().
  if (extension->RequiresSortOrdinal() &&
      (install_flags & kInstallFlagIsEphemeral) == 0) {
    extension_dict->SetString(
        kPrefSuggestedPageOrdinal,
        page_ordinal.IsValid() ? page_ordinal.ToInternalValue()
                               : std::string());
  }
  extension_dict->SetInteger(kDelayedInstallReason,
                             static_cast<int>(delay_reason));

  UpdateExtensionPref(extension->id(), kDelayedInstallInfo, extension_dict);
}

bool ExtensionPrefs::RemoveDelayedInstallInfo(
    const std::string& extension_id) {
  if (!GetExtensionPref(extension_id))
    return false;
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  bool result = update->Remove(kDelayedInstallInfo, NULL);
  return result;
}

bool ExtensionPrefs::FinishDelayedInstallInfo(
    const std::string& extension_id) {
  CHECK(Extension::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  base::DictionaryValue* extension_dict = update.Get();
  base::DictionaryValue* pending_install_dict = NULL;
  if (!extension_dict->GetDictionary(kDelayedInstallInfo,
                                     &pending_install_dict)) {
    return false;
  }

  // Retrieve and clear transient values populated by SetDelayedInstallInfo().
  // Also do any other data cleanup that makes sense.
  std::string serialized_ordinal;
  syncer::StringOrdinal suggested_page_ordinal;
  bool needs_sort_ordinal = false;
  if (pending_install_dict->GetString(kPrefSuggestedPageOrdinal,
                                      &serialized_ordinal)) {
    suggested_page_ordinal = syncer::StringOrdinal(serialized_ordinal);
    needs_sort_ordinal = true;
    pending_install_dict->Remove(kPrefSuggestedPageOrdinal, NULL);
  }
  pending_install_dict->Remove(kDelayedInstallReason, NULL);

  const base::Time install_time = time_provider_->GetCurrentTime();
  pending_install_dict->Set(
      kPrefInstallTime,
      new base::StringValue(
          base::Int64ToString(install_time.ToInternalValue())));

  // Some extension pref values are written conditionally. If they are not
  // present in the delayed install data, they should be removed when the
  // delayed install is committed.
  extension_dict->Remove(kPrefEphemeralApp, NULL);

  // Commit the delayed install data.
  for (base::DictionaryValue::Iterator it(*pending_install_dict); !it.IsAtEnd();
       it.Advance()) {
    extension_dict->Set(it.key(), it.value().DeepCopy());
  }
  FinishExtensionInfoPrefs(extension_id, install_time, needs_sort_ordinal,
                           suggested_page_ordinal, extension_dict);
  return true;
}

scoped_ptr<ExtensionInfo> ExtensionPrefs::GetDelayedInstallInfo(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension_prefs =
      GetExtensionPref(extension_id);
  if (!extension_prefs)
    return scoped_ptr<ExtensionInfo>();

  const base::DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kDelayedInstallInfo, &ext))
    return scoped_ptr<ExtensionInfo>();

  return GetInstalledInfoHelper(extension_id, ext);
}

ExtensionPrefs::DelayReason ExtensionPrefs::GetDelayedInstallReason(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension_prefs =
      GetExtensionPref(extension_id);
  if (!extension_prefs)
    return DELAY_REASON_NONE;

  const base::DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kDelayedInstallInfo, &ext))
    return DELAY_REASON_NONE;

  int delay_reason;
  if (!ext->GetInteger(kDelayedInstallReason, &delay_reason))
    return DELAY_REASON_NONE;

  return static_cast<DelayReason>(delay_reason);
}

scoped_ptr<ExtensionPrefs::ExtensionsInfo> ExtensionPrefs::
    GetAllDelayedInstallInfo() const {
  scoped_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  for (base::DictionaryValue::Iterator extension_id(*extensions);
       !extension_id.IsAtEnd(); extension_id.Advance()) {
    if (!Extension::IdIsValid(extension_id.key()))
      continue;

    scoped_ptr<ExtensionInfo> info = GetDelayedInstallInfo(extension_id.key());
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info.release()));
  }

  return extensions_info.Pass();
}

bool ExtensionPrefs::IsEphemeralApp(const std::string& extension_id) const {
  if (ReadPrefAsBooleanAndReturn(extension_id, kPrefEphemeralApp))
    return true;

  // Ephemerality was previously stored in the creation flags, so we must also
  // check it for backcompatibility.
  return (GetCreationFlags(extension_id) & Extension::IS_EPHEMERAL) != 0;
}

void ExtensionPrefs::OnEphemeralAppPromoted(const std::string& extension_id) {
  DCHECK(IsEphemeralApp(extension_id));

  UpdateExtensionPref(extension_id, kPrefEphemeralApp, NULL);

  // Ephemerality was previously stored in the creation flags, so ensure the bit
  // is cleared.
  int creation_flags = Extension::NO_FLAGS;
  if (ReadPrefAsInteger(extension_id, kPrefCreationFlags, &creation_flags)) {
    if (creation_flags & Extension::IS_EPHEMERAL) {
      creation_flags &= ~static_cast<int>(Extension::IS_EPHEMERAL);
      UpdateExtensionPref(extension_id,
                          kPrefCreationFlags,
                          new base::FundamentalValue(creation_flags));
    }
  }
}

bool ExtensionPrefs::WasAppDraggedByUser(const std::string& extension_id) {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefUserDraggedApp);
}

void ExtensionPrefs::SetAppDraggedByUser(const std::string& extension_id) {
  UpdateExtensionPref(extension_id, kPrefUserDraggedApp,
                      new base::FundamentalValue(true));
}

bool ExtensionPrefs::IsFromWebStore(
    const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefFromWebStore, &result))
    return result;
  return false;
}

bool ExtensionPrefs::IsFromBookmark(
    const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefFromBookmark, &result))
    return result;
  return false;
}

int ExtensionPrefs::GetCreationFlags(const std::string& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  if (!ReadPrefAsInteger(extension_id, kPrefCreationFlags, &creation_flags)) {
    // Since kPrefCreationFlags was added later, it will be missing for
    // previously installed extensions.
    if (IsFromBookmark(extension_id))
      creation_flags |= Extension::FROM_BOOKMARK;
    if (IsFromWebStore(extension_id))
      creation_flags |= Extension::FROM_WEBSTORE;
    if (WasInstalledByDefault(extension_id))
      creation_flags |= Extension::WAS_INSTALLED_BY_DEFAULT;
    if (WasInstalledByOem(extension_id))
      creation_flags |= Extension::WAS_INSTALLED_BY_OEM;
  }
  return creation_flags;
}

int ExtensionPrefs::GetDelayedInstallCreationFlags(
    const std::string& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  const base::DictionaryValue* delayed_info = NULL;
  if (ReadPrefAsDictionary(extension_id, kDelayedInstallInfo, &delayed_info)) {
    delayed_info->GetInteger(kPrefCreationFlags, &creation_flags);
  }
  return creation_flags;
}

bool ExtensionPrefs::WasInstalledByDefault(
    const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary &&
      dictionary->GetBoolean(kPrefWasInstalledByDefault, &result))
    return result;
  return false;
}

bool ExtensionPrefs::WasInstalledByOem(const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefWasInstalledByOem, &result))
    return result;
  return false;
}

base::Time ExtensionPrefs::GetInstallTime(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
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

bool ExtensionPrefs::DoNotSync(const std::string& extension_id) const {
  bool do_not_sync;
  if (!ReadPrefAsBoolean(extension_id, kPrefDoNotSync, &do_not_sync))
    return false;

  return do_not_sync;
}

base::Time ExtensionPrefs::GetLastLaunchTime(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return base::Time();

  std::string launch_time_str;
  if (!extension->GetString(kPrefLastLaunchTime, &launch_time_str))
    return base::Time();
  int64 launch_time_i64 = 0;
  if (!base::StringToInt64(launch_time_str, &launch_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(launch_time_i64);
}

void ExtensionPrefs::SetLastLaunchTime(const std::string& extension_id,
                                       const base::Time& time) {
  DCHECK(Extension::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get(), kPrefLastLaunchTime, time);
}

void ExtensionPrefs::GetExtensions(ExtensionIdList* out) {
  CHECK(out);

  scoped_ptr<ExtensionsInfo> extensions_info(GetInstalledExtensionsInfo());

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    out->push_back(info->extension_id);
  }
}

// static
ExtensionIdList ExtensionPrefs::GetExtensionsFrom(
    const PrefService* pref_service) {
  ExtensionIdList result;

  const base::DictionaryValue* extension_prefs = NULL;
  const base::Value* extension_prefs_value =
      pref_service->GetUserPrefValue(pref_names::kExtensions);
  if (!extension_prefs_value ||
      !extension_prefs_value->GetAsDictionary(&extension_prefs)) {
    return result;  // Empty set
  }

  for (base::DictionaryValue::Iterator it(*extension_prefs); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* ext = NULL;
    if (!it.value().GetAsDictionary(&ext)) {
      NOTREACHED() << "Invalid pref for extension " << it.key();
      continue;
    }
    if (!IsBlacklistBitSet(ext))
      result.push_back(it.key());
  }
  return result;
}

void ExtensionPrefs::AddObserver(ExtensionPrefsObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionPrefs::RemoveObserver(ExtensionPrefsObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ExtensionPrefs::FixMissingPrefs(const ExtensionIdList& extension_ids) {
  // Fix old entries that did not get an installation time entry when they
  // were installed or don't have a preferences field.
  for (ExtensionIdList::const_iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    if (GetInstallTime(*ext_id) == base::Time()) {
      VLOG(1) << "Could not parse installation time of extension "
              << *ext_id << ". It was probably installed before setting "
              << kPrefInstallTime << " was introduced. Updating "
              << kPrefInstallTime << " to the current time.";
      const base::Time install_time = time_provider_->GetCurrentTime();
      UpdateExtensionPref(*ext_id,
                          kPrefInstallTime,
                          new base::StringValue(base::Int64ToString(
                              install_time.ToInternalValue())));
    }
  }
}

void ExtensionPrefs::InitPrefStore() {
  if (extensions_disabled_) {
    extension_pref_value_map_->NotifyInitializationCompleted();
    return;
  }

  // When this is called, the PrefService is initialized and provides access
  // to the user preferences stored in a JSON file.
  ExtensionIdList extension_ids;
  GetExtensions(&extension_ids);
  // Create empty preferences dictionary for each extension (these dictionaries
  // are pruned when persisting the preferences to disk).
  for (ExtensionIdList::iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    ScopedExtensionPrefUpdate update(prefs_, *ext_id);
    // This creates an empty dictionary if none is stored.
    update.Get();
  }

  FixMissingPrefs(extension_ids);
  MigratePermissions(extension_ids);
  MigrateDisableReasons(extension_ids);
  app_sorting_->Initialize(extension_ids);

  InitExtensionControlledPrefs(extension_pref_value_map_);

  extension_pref_value_map_->NotifyInitializationCompleted();
}

bool ExtensionPrefs::HasIncognitoPrefValue(const std::string& pref_key) {
  bool has_incognito_pref_value = false;
  extension_pref_value_map_->GetEffectivePrefValue(pref_key,
                                                   true,
                                                   &has_incognito_pref_value);
  return has_incognito_pref_value;
}

URLPatternSet ExtensionPrefs::GetAllowedInstallSites() {
  URLPatternSet result;
  const base::ListValue* list =
      prefs_->GetList(pref_names::kAllowedInstallSites);
  CHECK(list);

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string entry_string;
    URLPattern entry(URLPattern::SCHEME_ALL);
    if (!list->GetString(i, &entry_string) ||
        entry.Parse(entry_string) != URLPattern::PARSE_SUCCESS) {
      LOG(ERROR) << "Invalid value for preference: "
                 << pref_names::kAllowedInstallSites << "." << i;
      continue;
    }
    result.AddPattern(entry);
  }

  return result;
}

const base::DictionaryValue* ExtensionPrefs::GetGeometryCache(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return NULL;

  const base::DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kPrefGeometryCache, &ext))
    return NULL;

  return ext;
}

void ExtensionPrefs::SetGeometryCache(
    const std::string& extension_id,
    scoped_ptr<base::DictionaryValue> cache) {
  UpdateExtensionPref(extension_id, kPrefGeometryCache, cache.release());
}

const base::DictionaryValue* ExtensionPrefs::GetInstallSignature() {
  return prefs_->GetDictionary(kInstallSignature);
}

void ExtensionPrefs::SetInstallSignature(
    const base::DictionaryValue* signature) {
  if (signature) {
    prefs_->Set(kInstallSignature, *signature);
    DVLOG(1) << "SetInstallSignature - saving";
  } else {
    DVLOG(1) << "SetInstallSignature - clearing";
    prefs_->ClearPref(kInstallSignature);
  }
}

std::string ExtensionPrefs::GetInstallParam(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)  // Expected during unit testing.
    return std::string();
  std::string install_parameter;
  if (!extension->GetString(kPrefInstallParam, &install_parameter))
    return std::string();
  return install_parameter;
}

void ExtensionPrefs::SetInstallParam(const std::string& extension_id,
                                     const std::string& install_parameter) {
  UpdateExtensionPref(extension_id,
                      kPrefInstallParam,
                      new base::StringValue(install_parameter));
}

int ExtensionPrefs::GetCorruptedDisableCount() {
  return prefs_->GetInteger(kCorruptedDisableCount);
}

void ExtensionPrefs::IncrementCorruptedDisableCount() {
  int count = prefs_->GetInteger(kCorruptedDisableCount);
  prefs_->SetInteger(kCorruptedDisableCount, count + 1);
}

ExtensionPrefs::ExtensionPrefs(
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    scoped_ptr<AppSorting> app_sorting,
    scoped_ptr<TimeProvider> time_provider,
    bool extensions_disabled,
    const std::vector<ExtensionPrefsObserver*>& early_observers)
    : prefs_(prefs),
      install_directory_(root_dir),
      extension_pref_value_map_(extension_pref_value_map),
      app_sorting_(app_sorting.Pass()),
      time_provider_(time_provider.Pass()),
      extensions_disabled_(extensions_disabled) {
  app_sorting_->SetExtensionScopedPrefs(this);
  MakePathsRelative();

  // Ensure that any early observers are watching before prefs are initialized.
  for (std::vector<ExtensionPrefsObserver*>::const_iterator iter =
           early_observers.begin();
       iter != early_observers.end();
       ++iter) {
    AddObserver(*iter);
  }

  InitPrefStore();
}

void ExtensionPrefs::SetNeedsStorageGarbageCollection(bool value) {
  prefs_->SetBoolean(pref_names::kStorageGarbageCollect, value);
}

bool ExtensionPrefs::NeedsStorageGarbageCollection() {
  return prefs_->GetBoolean(pref_names::kStorageGarbageCollect);
}

// static
void ExtensionPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      pref_names::kExtensions,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kToolbar,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      pref_names::kToolbarSize,
      -1,  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kExtensionsBlacklistUpdate,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kInstallAllowList,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kInstallDenyList,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      pref_names::kInstallForceList,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kAllowedTypes,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      pref_names::kStorageGarbageCollect,
      false,  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      pref_names::kLastUpdateCheck,
      0,  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      pref_names::kNextUpdateCheck,
      0,  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kAllowedInstallSites,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      pref_names::kLastChromeVersion,
      std::string(),  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kInstallSignature,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterListPref(pref_names::kNativeMessagingBlacklist,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kNativeMessagingWhitelist,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      pref_names::kNativeMessagingUserLevelHosts,
      true,  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kCorruptedDisableCount,
      0,  // default value
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

template <class ExtensionIdContainer>
bool ExtensionPrefs::GetUserExtensionPrefIntoContainer(
    const char* pref,
    ExtensionIdContainer* id_container_out) {
  DCHECK(id_container_out->empty());

  const base::Value* user_pref_value = prefs_->GetUserPrefValue(pref);
  const base::ListValue* user_pref_as_list;
  if (!user_pref_value || !user_pref_value->GetAsList(&user_pref_as_list))
    return false;

  std::insert_iterator<ExtensionIdContainer> insert_iterator(
      *id_container_out, id_container_out->end());
  std::string extension_id;
  for (base::ListValue::const_iterator value_it = user_pref_as_list->begin();
       value_it != user_pref_as_list->end(); ++value_it) {
    if (!(*value_it)->GetAsString(&extension_id)) {
      NOTREACHED();
      continue;
    }
    insert_iterator = extension_id;
  }
  return true;
}

template <class ExtensionIdContainer>
void ExtensionPrefs::SetExtensionPrefFromContainer(
    const char* pref,
    const ExtensionIdContainer& strings) {
  ListPrefUpdate update(prefs_, pref);
  base::ListValue* list_of_values = update.Get();
  list_of_values->Clear();
  for (typename ExtensionIdContainer::const_iterator iter = strings.begin();
       iter != strings.end(); ++iter) {
    list_of_values->Append(new base::StringValue(*iter));
  }
}

void ExtensionPrefs::PopulateExtensionInfoPrefs(
    const Extension* extension,
    const base::Time install_time,
    Extension::State initial_state,
    int install_flags,
    const std::string& install_parameter,
    base::DictionaryValue* extension_dict) {
  // Leave the state blank for component extensions so that old chrome versions
  // loading new profiles do not fail in GetInstalledExtensionInfo. Older
  // Chrome versions would only check for an omitted state.
  if (initial_state != Extension::ENABLED_COMPONENT)
    extension_dict->Set(kPrefState, new base::FundamentalValue(initial_state));

  extension_dict->Set(kPrefLocation,
                      new base::FundamentalValue(extension->location()));
  extension_dict->Set(kPrefCreationFlags,
                      new base::FundamentalValue(extension->creation_flags()));
  extension_dict->Set(kPrefFromWebStore,
                      new base::FundamentalValue(extension->from_webstore()));
  extension_dict->Set(kPrefFromBookmark,
                      new base::FundamentalValue(extension->from_bookmark()));
  extension_dict->Set(
      kPrefWasInstalledByDefault,
      new base::FundamentalValue(extension->was_installed_by_default()));
  extension_dict->Set(
      kPrefWasInstalledByOem,
      new base::FundamentalValue(extension->was_installed_by_oem()));
  extension_dict->Set(kPrefInstallTime,
                      new base::StringValue(
                          base::Int64ToString(install_time.ToInternalValue())));
  if (install_flags & kInstallFlagIsBlacklistedForMalware)
    extension_dict->Set(kPrefBlacklist, new base::FundamentalValue(true));

  if (install_flags & kInstallFlagIsEphemeral)
    extension_dict->Set(kPrefEphemeralApp, new base::FundamentalValue(true));
  else
    extension_dict->Remove(kPrefEphemeralApp, NULL);

  base::FilePath::StringType path = MakePathRelative(install_directory_,
                                                     extension->path());
  extension_dict->Set(kPrefPath, new base::StringValue(path));
  if (!install_parameter.empty()) {
    extension_dict->Set(kPrefInstallParam,
                        new base::StringValue(install_parameter));
  }
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    extension_dict->Set(kPrefManifest,
                        extension->manifest()->value()->DeepCopy());
  }

  // Only writes kPrefDoNotSync when it is not the default.
  if (install_flags & kInstallFlagDoNotSync)
    extension_dict->Set(kPrefDoNotSync, new base::FundamentalValue(true));
  else
    extension_dict->Remove(kPrefDoNotSync, NULL);
}

void ExtensionPrefs::InitExtensionControlledPrefs(
    ExtensionPrefValueMap* value_map) {
  ExtensionIdList extension_ids;
  GetExtensions(&extension_ids);

  for (ExtensionIdList::iterator extension_id = extension_ids.begin();
       extension_id != extension_ids.end();
       ++extension_id) {
    base::Time install_time = GetInstallTime(*extension_id);
    bool is_enabled = !IsExtensionDisabled(*extension_id);
    bool is_incognito_enabled = IsIncognitoEnabled(*extension_id);
    value_map->RegisterExtension(
        *extension_id, install_time, is_enabled, is_incognito_enabled);

    FOR_EACH_OBSERVER(
        ExtensionPrefsObserver,
        observer_list_,
        OnExtensionRegistered(*extension_id, install_time, is_enabled));

    // Set regular extension controlled prefs.
    LoadExtensionControlledPrefs(
        this, value_map, *extension_id, kExtensionPrefsScopeRegular);
    // Set incognito extension controlled prefs.
    LoadExtensionControlledPrefs(this,
                                 value_map,
                                 *extension_id,
                                 kExtensionPrefsScopeIncognitoPersistent);
    // Set regular-only extension controlled prefs.
    LoadExtensionControlledPrefs(
        this, value_map, *extension_id, kExtensionPrefsScopeRegularOnly);

    FOR_EACH_OBSERVER(ExtensionPrefsObserver,
                      observer_list_,
                      OnExtensionPrefsLoaded(*extension_id, this));
  }
}

void ExtensionPrefs::FinishExtensionInfoPrefs(
    const std::string& extension_id,
    const base::Time install_time,
    bool needs_sort_ordinal,
    const syncer::StringOrdinal& suggested_page_ordinal,
    base::DictionaryValue* extension_dict) {
  // Reinitializes various preferences with empty dictionaries.
  if (!extension_dict->HasKey(pref_names::kPrefPreferences)) {
    extension_dict->Set(pref_names::kPrefPreferences,
                        new base::DictionaryValue);
  }

  if (!extension_dict->HasKey(pref_names::kPrefIncognitoPreferences)) {
    extension_dict->Set(pref_names::kPrefIncognitoPreferences,
                        new base::DictionaryValue);
  }

  if (!extension_dict->HasKey(pref_names::kPrefRegularOnlyPreferences)) {
    extension_dict->Set(pref_names::kPrefRegularOnlyPreferences,
                        new base::DictionaryValue);
  }

  if (!extension_dict->HasKey(pref_names::kPrefContentSettings))
    extension_dict->Set(pref_names::kPrefContentSettings, new base::ListValue);

  if (!extension_dict->HasKey(pref_names::kPrefIncognitoContentSettings)) {
    extension_dict->Set(pref_names::kPrefIncognitoContentSettings,
                        new base::ListValue);
  }

  // If this point has been reached, any pending installs should be considered
  // out of date.
  extension_dict->Remove(kDelayedInstallInfo, NULL);

  // Clear state that may be registered from a previous install.
  extension_dict->Remove(EventRouter::kRegisteredEvents, NULL);

  // FYI, all code below here races on sudden shutdown because |extension_dict|,
  // |app_sorting_|, |extension_pref_value_map_|, and (potentially) observers
  // are updated non-transactionally. This is probably not fixable without
  // nested transactional updates to pref dictionaries.
  if (needs_sort_ordinal)
    app_sorting_->EnsureValidOrdinals(extension_id, suggested_page_ordinal);

  bool is_enabled = false;
  int initial_state;
  if (extension_dict->GetInteger(kPrefState, &initial_state)) {
    is_enabled = initial_state == Extension::ENABLED;
  }
  bool is_incognito_enabled = IsIncognitoEnabled(extension_id);

  extension_pref_value_map_->RegisterExtension(
      extension_id, install_time, is_enabled, is_incognito_enabled);

  FOR_EACH_OBSERVER(
      ExtensionPrefsObserver,
      observer_list_,
      OnExtensionRegistered(extension_id, install_time, is_enabled));
}

}  // namespace extensions
