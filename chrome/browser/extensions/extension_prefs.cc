// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs.h"

#include "base/command_line.h"
#include "base/prefs/pref_notifier.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/extensions/admin_policy.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/url_pattern.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif  // OS_WIN

namespace extensions {

namespace {

// Additional preferences keys

// The file entries that an extension had permission to access.
const char kFileEntries[] = "file_entries";

// The path to a file entry that an extension had permission to access.
const char kFileEntryPath[] = "path";

// Whether or not an extension had write access to a file entry.
const char kFileEntryWritable[] = "writable";

// Whether this extension was running when chrome last shutdown.
const char kPrefRunning[] = "running";

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

// The oauth client id used for app notification setup.
const char kPrefAppNotificationClientId[] = "app_notif_client_id";

// Indicates whether the user has disabled notifications or not.
const char kPrefAppNotificationDisbaled[] = "app_notif_disabled";

// The count of how many times we prompted the user to acknowledge an
// extension.
const char kPrefAcknowledgePromptCount[] = "ack_prompt_count";

// If true, this extension should be excluded from the sideload wipeout UI.
const char kPrefExcludeFromSideloadWipeout[] = "exclude_from_sideload_wipeout";

// Indicates whether the user has acknowledged various types of extensions.
const char kPrefExternalAcknowledged[] = "ack_external";
const char kPrefBlacklistAcknowledged[] = "ack_blacklist";

// Indicates whether to show an install warning when the user enables.
const char kExtensionDidEscalatePermissions[] = "install_warning_on_enable";

// DO NOT USE, use kPrefDisableReasons instead.
// Indicates whether the extension was updated while it was disabled.
const char kDeprecatedPrefDisableReason[] = "disable_reason";

// A bitmask of all the reasons an extension is disabled.
const char kPrefDisableReasons[] = "disable_reasons";

// A preference that tracks browser action toolbar configuration. This is a list
// object stored in the Preferences file. The extensions are stored by ID.
const char kExtensionToolbar[] = "extensions.toolbar";

// A preference that tracks the order of extensions in an action box
// (list of extension ids).
const char kExtensionActionBox[] = "extensions.action_box_order";

// A preference that tracks the order of extensions in a toolbar when
// action box is enabled (list of extension ids).
const char kExtensionActionBoxBar[] =
    "extensions.toolbar_order_with_action_box";

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

// A preference set by the web store to indicate login information for
// purchased apps.
const char kWebStoreLogin[] = "extensions.webstore_login";

// A preference set by the the NTP to persist the desired launch container type
// used for apps.
const char kPrefLaunchType[] = "launchType";

// A preference specifying if the user dragged the app on the NTP.
const char kPrefUserDraggedApp[] = "user_dragged_app_ntp";

// A preference for storing extra data sent in update checks for an extension.
const char kUpdateUrlData[] = "update_url_data";

// Whether the browser action is visible in the toolbar.
const char kBrowserActionVisible[] = "browser_action_visible";

// Preferences that hold which permissions the user has granted the extension.
// We explicitly keep track of these so that extensions can contain unknown
// permissions, for backwards compatibility reasons, and we can still prompt
// the user to accept them once recognized. We store the active permission
// permissions because they may differ from those defined in the manifest.
const char kPrefActivePermissions[] = "active_permissions";
const char kPrefGrantedPermissions[] = "granted_permissions";

// The preference names for PermissionSet values.
const char kPrefAPIs[] = "api";
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

// A prefrence that indicates whethere the extension was installed as
// default apps.
const char kPrefWasInstalledByDefault[] = "was_installed_by_default";

// A preference that contains any extension-controlled preferences.
const char kPrefPreferences[] = "preferences";

// A preference that contains any extension-controlled incognito preferences.
const char kPrefIncognitoPreferences[] = "incognito_preferences";

// A preference that contains any extension-controlled regular-only preferences.
const char kPrefRegularOnlyPreferences[] = "regular_only_preferences";

// A preference that contains extension-set content settings.
const char kPrefContentSettings[] = "content_settings";

// A preference that contains extension-set content settings.
const char kPrefIncognitoContentSettings[] = "incognito_content_settings";

// A list of event names that this extension has registered from its lazy
// background page.
const char kRegisteredEvents[] = "events";

// A dictionary of event names to lists of filters that this extension has
// registered from its lazy background page.
const char kFilteredEvents[] = "filtered_events";

// Persisted value for omnibox.setDefaultSuggestion.
const char kOmniboxDefaultSuggestion[] = "omnibox_default_suggestion";

// List of media gallery permissions.
const char kMediaGalleriesPermissions[] = "media_galleries_permissions";

// Key for Media Gallery ID.
const char kMediaGalleryIdKey[] = "id";

// Key for Media Gallery Permission Value.
const char kMediaGalleryHasPermissionKey[] = "has_permission";

// Key for Geometry Cache preference.
const char kPrefGeometryCache[] = "geometry_cache";

// Key for what version chrome was last time the extension prefs were loaded.
const char kExtensionsLastChromeVersion[] = "extensions.last_chrome_version";

// Key for whether the sideload wipeout effort is done.
const char kSideloadWipeoutDone[] = "extensions.sideload_wipeout_done";

// Provider of write access to a dictionary storing extension prefs.
class ScopedExtensionPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedExtensionPrefUpdate(PrefService* service,
                            const std::string& extension_id) :
    DictionaryPrefUpdate(service, ExtensionPrefs::kExtensionsPref),
    extension_id_(extension_id) {}

  virtual ~ScopedExtensionPrefUpdate() {
  }

  // DictionaryPrefUpdate overrides:
  virtual DictionaryValue* Get() OVERRIDE {
    DictionaryValue* dict = DictionaryPrefUpdate::Get();
    DictionaryValue* extension = NULL;
    if (!dict->GetDictionary(extension_id_, &extension)) {
      // Extension pref does not exist, create it.
      extension = new DictionaryValue();
      dict->SetWithoutPathExpansion(extension_id_, extension);
    }
    return extension;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionPrefUpdate);
};

// Provider of write access to a dictionary storing extension controlled prefs.
class ScopedExtensionControlledPrefUpdate : public ScopedExtensionPrefUpdate {
 public:
  // |incognito_or_regular_path| indicates the sub-path where the
  // extension controlled preferences are stored. This has to be a persistent
  // scope (i.e. one of kPrefPreferences, kPrefRegularOnlyPreferences or
  // kPrefIncognitoPreferences).
  ScopedExtensionControlledPrefUpdate(
      PrefService* service,
      const std::string& extension_id,
      const std::string& incognito_or_regular_path)
      : ScopedExtensionPrefUpdate(service, extension_id),
        incognito_or_regular_path_(incognito_or_regular_path) {}

  virtual ~ScopedExtensionControlledPrefUpdate() {
  }

  // ScopedExtensionPrefUpdate overrides:
  virtual DictionaryValue* Get() OVERRIDE {
    DictionaryValue* dict = ScopedExtensionPrefUpdate::Get();
    DictionaryValue* preferences = NULL;
    if (!dict->GetDictionary(incognito_or_regular_path_, &preferences)) {
      preferences = new DictionaryValue;
      dict->SetWithoutPathExpansion(incognito_or_regular_path_, preferences);
    }
    return preferences;
  }

 private:
  const std::string incognito_or_regular_path_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionControlledPrefUpdate);
};

std::string JoinPrefs(const std::string& parent, const char* child) {
  return parent + "." + child;
}

bool ScopeToPrefKey(ExtensionPrefsScope scope, std::string* result) {
  switch (scope) {
    case kExtensionPrefsScopeRegular:
      *result = kPrefPreferences;
      return true;
    case kExtensionPrefsScopeRegularOnly:
      *result = kPrefRegularOnlyPreferences;
      return true;
    case kExtensionPrefsScopeIncognitoPersistent:
      *result = kPrefIncognitoPreferences;
      return true;
    case kExtensionPrefsScopeIncognitoSessionOnly:
      return false;
  }
  NOTREACHED();
  return false;
}

const char* GetToolbarOrderKeyName() {
  return FeatureSwitch::extensions_in_action_box()->IsEnabled() ?
      kExtensionActionBoxBar : kExtensionToolbar;
}

// Reads a boolean pref from |ext| with key |pref_key|.
// Return false if the value is false or |pref_key| does not exist.
bool ReadBooleanFromPref(const DictionaryValue* ext,
                         const std::string& pref_key) {
  bool bool_value = false;
  ext->GetBoolean(pref_key, &bool_value);
  return bool_value;
}

// Reads an integer pref from |ext| with key |pref_key|.
// Return false if the value does not exist.
bool ReadIntegerFromPref(const DictionaryValue* ext,
                         const std::string& pref_key,
                         int* out_value) {
  if (!ext->GetInteger(pref_key, out_value))
    return false;
  return out_value != NULL;
}

// Checks if kPrefBlacklist is set to true in the DictionaryValue.
// Return false if the value is false or kPrefBlacklist does not exist.
// This is used to decide if an extension is blacklisted.
bool IsBlacklistBitSet(const DictionaryValue* ext) {
  return ReadBooleanFromPref(ext, kPrefBlacklist);
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
// ExtensionPrefs
//

// static
scoped_ptr<ExtensionPrefs> ExtensionPrefs::Create(
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    bool extensions_disabled) {
  return ExtensionPrefs::Create(prefs,
                                root_dir,
                                extension_pref_value_map,
                                extensions_disabled,
                                make_scoped_ptr(new TimeProvider()));
}

// static
scoped_ptr<ExtensionPrefs> ExtensionPrefs::Create(
    PrefService* pref_service,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    bool extensions_disabled,
    scoped_ptr<TimeProvider> time_provider) {
  scoped_ptr<ExtensionPrefs> prefs(
      new ExtensionPrefs(pref_service,
                         root_dir,
                         extension_pref_value_map,
                         time_provider.Pass()));
  prefs->Init(extensions_disabled);
  return prefs.Pass();
}

ExtensionPrefs::~ExtensionPrefs() {
}

// static
const char ExtensionPrefs::kExtensionsPref[] = "extensions.settings";

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
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict || dict->empty())
    return;

  // Collect all extensions ids with absolute paths in |absolute_keys|.
  std::set<std::string> absolute_keys;
  for (DictionaryValue::key_iterator i = dict->begin_keys();
       i != dict->end_keys(); ++i) {
    const DictionaryValue* extension_dict = NULL;
    if (!dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict))
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
      absolute_keys.insert(*i);
  }
  if (absolute_keys.empty())
    return;

  // Fix these paths.
  DictionaryPrefUpdate update(prefs_, kExtensionsPref);
  DictionaryValue* update_dict = update.Get();
  for (std::set<std::string>::iterator i = absolute_keys.begin();
       i != absolute_keys.end(); ++i) {
    DictionaryValue* extension_dict = NULL;
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

bool ExtensionPrefs::ReadExtensionPrefBoolean(
    const std::string& extension_id, const std::string& pref_key) const {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext) {
    // No such extension yet.
    return false;
  }
  return ReadBooleanFromPref(ext, pref_key);
}

bool ExtensionPrefs::ReadExtensionPrefInteger(
    const std::string& extension_id, const std::string& pref_key,
    int* out_value) const {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext) {
    // No such extension yet.
    return false;
  }
  return ReadIntegerFromPref(ext, pref_key, out_value);
}

bool ExtensionPrefs::ReadExtensionPrefList(
    const std::string& extension_id, const std::string& pref_key,
    const ListValue** out_value) const {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  const ListValue* out = NULL;
  if (!ext || !ext->GetList(pref_key, &out))
    return false;
  if (out_value)
    *out_value = out;

  return true;
}

bool ExtensionPrefs::ReadExtensionPrefString(
    const std::string& extension_id, const std::string& pref_key,
    std::string* out_value) const {
  const DictionaryValue* ext = GetExtensionPref(extension_id);

  if (!ext || !ext->GetString(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadExtensionPrefURLPatternSet(
    const std::string& extension_id,
    const std::string& pref_key,
    URLPatternSet* result,
    int valid_schemes) {
  const ListValue* value = NULL;
  if (!ReadExtensionPrefList(extension_id, pref_key, &value))
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

PermissionSet* ExtensionPrefs::ReadExtensionPrefPermissionSet(
    const std::string& extension_id,
    const std::string& pref_key) {
  if (!GetExtensionPref(extension_id))
    return NULL;

  // Retrieve the API permissions. Please refer SetExtensionPrefPermissionSet()
  // for api_values format.
  APIPermissionSet apis;
  const ListValue* api_values = NULL;
  std::string api_pref = JoinPrefs(pref_key, kPrefAPIs);
  if (ReadExtensionPrefList(extension_id, api_pref, &api_values)) {
    APIPermissionSet::ParseFromJSON(api_values, &apis, NULL, NULL);
  }

  // Retrieve the explicit host permissions.
  URLPatternSet explicit_hosts;
  ReadExtensionPrefURLPatternSet(
      extension_id, JoinPrefs(pref_key, kPrefExplicitHosts),
      &explicit_hosts, Extension::kValidHostPermissionSchemes);

  // Retrieve the scriptable host permissions.
  URLPatternSet scriptable_hosts;
  ReadExtensionPrefURLPatternSet(
      extension_id, JoinPrefs(pref_key, kPrefScriptableHosts),
      &scriptable_hosts, UserScript::kValidUserScriptSchemes);

  return new PermissionSet(apis, explicit_hosts, scriptable_hosts);
}

void ExtensionPrefs::SetExtensionPrefPermissionSet(
    const std::string& extension_id,
    const std::string& pref_key,
    const PermissionSet* new_value) {
  // Set the API permissions.
  // The format of api_values is:
  // [ "permission_name1",   // permissions do not support detail.
  //   "permission_name2",
  //   {"permission_name3": value },
  //   // permission supports detail, permission detail will be stored in value.
  //   ...
  // ]
  ListValue* api_values = new ListValue();
  APIPermissionSet apis = new_value->apis();
  std::string api_pref = JoinPrefs(pref_key, kPrefAPIs);
  for (APIPermissionSet::const_iterator i = apis.begin();
       i != apis.end(); ++i) {
    scoped_ptr<Value> detail(i->ToValue());
    if (detail) {
      DictionaryValue* tmp = new DictionaryValue();
      tmp->Set(i->name(), detail.release());
      api_values->Append(tmp);
    } else {
      api_values->Append(Value::CreateStringValue(i->name()));
    }
  }
  UpdateExtensionPref(extension_id, api_pref, api_values);

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
  ReadExtensionPrefInteger(extension_id, kPrefAcknowledgePromptCount, &count);
  ++count;
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount,
                      Value::CreateIntegerValue(count));
  return count;
}

bool ExtensionPrefs::IsExternalExtensionAcknowledged(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefExternalAcknowledged);
}

bool ExtensionPrefs::IsExternalExtensionExcludedFromWipeout(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id,
                                  kPrefExcludeFromSideloadWipeout);
}

void ExtensionPrefs::AcknowledgeExternalExtension(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalAcknowledged,
                      Value::CreateBooleanValue(true));
  UpdateExtensionPref(extension_id, kPrefExcludeFromSideloadWipeout,
                      Value::CreateBooleanValue(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, NULL);
}

bool ExtensionPrefs::IsBlacklistedExtensionAcknowledged(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefBlacklistAcknowledged);
}

void ExtensionPrefs::AcknowledgeBlacklistedExtension(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefBlacklistAcknowledged,
                      Value::CreateBooleanValue(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, NULL);
}

bool ExtensionPrefs::SetAlertSystemFirstRun() {
  if (prefs_->GetBoolean(prefs::kExtensionAlertsInitializedPref)) {
    return true;
  }
  prefs_->SetBoolean(prefs::kExtensionAlertsInitializedPref, true);
  return false;
}

std::string ExtensionPrefs::GetAppNotificationClientId(
    const std::string& extension_id) const {
  const DictionaryValue* dict = GetExtensionPref(extension_id);
  if (!dict || !dict->HasKey(kPrefAppNotificationClientId))
    return std::string();

  std::string tmp;
  dict->GetString(kPrefAppNotificationClientId, &tmp);
  return tmp;
}

void ExtensionPrefs::SetAppNotificationClientId(
    const std::string& extension_id,
    const std::string& oauth_client_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefAppNotificationClientId,
                      Value::CreateStringValue(oauth_client_id));
}

bool ExtensionPrefs::IsAppNotificationDisabled(
    const std::string& extension_id) const {
  return ReadExtensionPrefBoolean(extension_id, kPrefAppNotificationDisbaled);
}

void ExtensionPrefs::SetAppNotificationDisabled(
    const std::string& extension_id, bool value) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefAppNotificationDisbaled,
                      Value::CreateBooleanValue(value));
}

bool ExtensionPrefs::ExtensionsBlacklistedByDefault() const {
  return admin_policy::BlacklistedByDefault(
      prefs_->GetList(prefs::kExtensionInstallDenyList));
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
}

int ExtensionPrefs::GetDisableReasons(const std::string& extension_id) {
  int value = -1;
  if (ReadExtensionPrefInteger(extension_id, kPrefDisableReasons, &value) &&
      value >= 0) {
    return value;
  }
  return Extension::DISABLE_NONE;
}

void ExtensionPrefs::AddDisableReason(const std::string& extension_id,
                                      Extension::DisableReason disable_reason) {
  int new_value = GetDisableReasons(extension_id) |
      static_cast<int>(disable_reason);
  UpdateExtensionPref(extension_id, kPrefDisableReasons,
                      Value::CreateIntegerValue(new_value));
}

void ExtensionPrefs::RemoveDisableReason(
    const std::string& extension_id,
    Extension::DisableReason disable_reason) {
  int new_value = GetDisableReasons(extension_id) &
      ~static_cast<int>(disable_reason);
  if (new_value == Extension::DISABLE_NONE) {
    UpdateExtensionPref(extension_id, kPrefDisableReasons, NULL);
  } else {
    UpdateExtensionPref(extension_id, kPrefDisableReasons,
                        Value::CreateIntegerValue(new_value));
  }
}

void ExtensionPrefs::ClearDisableReasons(const std::string& extension_id) {
  UpdateExtensionPref(extension_id, kPrefDisableReasons, NULL);
}

std::set<std::string> ExtensionPrefs::GetBlacklistedExtensions() {
  std::set<std::string> ids;

  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions)
    return ids;

  for (DictionaryValue::Iterator it(*extensions); it.HasNext(); it.Advance()) {
    if (!it.value().IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED() << "Invalid pref for extension " << it.key();
      continue;
    }
    if (IsBlacklistBitSet(static_cast<const DictionaryValue*>(&it.value())))
      ids.insert(it.key());
  }

  return ids;
}

void ExtensionPrefs::SetExtensionBlacklisted(const std::string& extension_id,
                                             bool is_blacklisted) {
  bool currently_blacklisted = IsExtensionBlacklisted(extension_id);
  if (is_blacklisted == currently_blacklisted) {
    NOTREACHED() << extension_id << " is " <<
                    (currently_blacklisted ? "already" : "not") <<
                    " blacklisted";
    return;
  }

  // Always make sure the "acknowledged" bit is cleared since the blacklist bit
  // is changing.
  UpdateExtensionPref(extension_id, kPrefBlacklistAcknowledged, NULL);

  if (is_blacklisted) {
    UpdateExtensionPref(extension_id,
                        kPrefBlacklist,
                        new base::FundamentalValue(true));
  } else {
    UpdateExtensionPref(extension_id, kPrefBlacklist, NULL);
    const DictionaryValue* dict = GetExtensionPref(extension_id);
    if (dict && dict->empty())
      DeleteExtensionPrefs(extension_id);
  }
}

bool ExtensionPrefs::IsExtensionBlacklisted(const std::string& id) const {
  const DictionaryValue* ext_prefs = GetExtensionPref(id);
  return ext_prefs && IsBlacklistBitSet(ext_prefs);
}

namespace {

// Serializes |time| as a string value mapped to |key| in |dictionary|.
void SaveTime(DictionaryValue* dictionary,
              const char* key,
              const base::Time& time) {
  if (!dictionary)
    return;
  std::string string_value = base::Int64ToString(time.ToInternalValue());
  dictionary->SetString(key, string_value);
}

// The opposite of SaveTime. If |key| is not found, this returns an empty Time
// (is_null() will return true).
base::Time ReadTime(const DictionaryValue* dictionary, const char* key) {
  if (!dictionary)
    return base::Time();
  std::string string_value;
  int64 value;
  if (dictionary->GetString(key, &string_value)) {
    if (base::StringToInt64(string_value, &value)) {
      return base::Time::FromInternalValue(value);
    }
  }
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
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kActiveBit, &result))
    return result;
  return false;
}

void ExtensionPrefs::SetActiveBit(const std::string& extension_id,
                                  bool active) {
  UpdateExtensionPref(extension_id, kActiveBit,
                      Value::CreateBooleanValue(active));
}

void ExtensionPrefs::MigratePermissions(const ExtensionIdList& extension_ids) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  for (ExtensionIdList::const_iterator ext_id =
       extension_ids.begin(); ext_id != extension_ids.end(); ++ext_id) {

    // An extension's granted permissions need to be migrated if the
    // full_access bit is present. This bit was always present in the previous
    // scheme and is never present now.
    bool full_access;
    const DictionaryValue* ext = GetExtensionPref(*ext_id);
    if (!ext || !ext->GetBoolean(kPrefOldGrantedFullAccess, &full_access))
      continue;

    // Remove the full access bit (empty list will get trimmed).
    UpdateExtensionPref(
        *ext_id, kPrefOldGrantedFullAccess, new ListValue());

    // Add the plugin permission if the full access bit was set.
    if (full_access) {
      const ListValue* apis = NULL;
      ListValue* new_apis = NULL;

      std::string granted_apis =
          JoinPrefs(kPrefGrantedPermissions, kPrefAPIs);
      if (ext->GetList(kPrefOldGrantedAPIs, &apis))
        new_apis = apis->DeepCopy();
      else
        new_apis = new ListValue();

      std::string plugin_name = info->GetByID(
          APIPermission::kPlugin)->name();
      new_apis->Append(Value::CreateStringValue(plugin_name));
      UpdateExtensionPref(*ext_id, granted_apis, new_apis);
    }

    // The granted permissions originally only held the effective hosts,
    // which are a combination of host and user script host permissions.
    // We now maintain these lists separately. For migration purposes, it
    // does not matter how we treat the old effective hosts as long as the
    // new effective hosts will be the same, so we move them to explicit
    // host permissions.
    const ListValue* hosts;
    std::string explicit_hosts =
        JoinPrefs(kPrefGrantedPermissions, kPrefExplicitHosts);
    if (ext->GetList(kPrefOldGrantedHosts, &hosts)) {
      UpdateExtensionPref(
          *ext_id, explicit_hosts, hosts->DeepCopy());

      // We can get rid of the old one by setting it to an empty list.
      UpdateExtensionPref(*ext_id, kPrefOldGrantedHosts, new ListValue());
    }
  }
}

void ExtensionPrefs::MigrateDisableReasons(
    const ExtensionIdList& extension_ids) {
  for (ExtensionIdList::const_iterator ext_id =
       extension_ids.begin(); ext_id != extension_ids.end(); ++ext_id) {
    int value = -1;
    if (ReadExtensionPrefInteger(*ext_id, kDeprecatedPrefDisableReason,
                                 &value)) {
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
                          Value::CreateIntegerValue(new_value));
      // Remove the old disable reason.
      UpdateExtensionPref(*ext_id, kDeprecatedPrefDisableReason, NULL);
    }
  }
}

void ExtensionPrefs::ClearRegisteredEvents() {
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions)
    return;

  for (DictionaryValue::key_iterator it = extensions->begin_keys();
       it != extensions->end_keys(); ++it) {
    UpdateExtensionPref(*it, kRegisteredEvents, NULL);
  }
}

PermissionSet* ExtensionPrefs::GetGrantedPermissions(
    const std::string& extension_id) {
  CHECK(Extension::IdIsValid(extension_id));
  return ReadExtensionPrefPermissionSet(extension_id, kPrefGrantedPermissions);
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
  return ReadExtensionPrefPermissionSet(extension_id, kPrefActivePermissions);
}

void ExtensionPrefs::SetActivePermissions(
    const std::string& extension_id,
    const PermissionSet* permissions) {
  SetExtensionPrefPermissionSet(
      extension_id, kPrefActivePermissions, permissions);
}

bool ExtensionPrefs::CheckRegisteredEventsUpToDate() {
  // If we're running inside a test, then assume prefs are all up-to-date.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return true;

  Version version;
  if (prefs_->HasPrefPath(kExtensionsLastChromeVersion)) {
    std::string version_str = prefs_->GetString(kExtensionsLastChromeVersion);
    version = Version(version_str);
  }

  chrome::VersionInfo current_version_info;
  std::string current_version = current_version_info.Version();
  prefs_->SetString(kExtensionsLastChromeVersion, current_version);

  // If there was no version string in prefs, assume we're out of date.
  if (!version.IsValid() || version.IsOlderThan(current_version)) {
    ClearRegisteredEvents();
    return false;
  }

  return true;
}

std::set<std::string> ExtensionPrefs::GetRegisteredEvents(
    const std::string& extension_id) {
  std::set<std::string> events;
  const DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return events;

  const ListValue* value = NULL;
  if (!extension->GetList(kRegisteredEvents, &value))
    return events;

  for (size_t i = 0; i < value->GetSize(); ++i) {
    std::string event;
    if (value->GetString(i, &event))
      events.insert(event);
  }
  return events;
}

void ExtensionPrefs::AddFilterToEvent(const std::string& event_name,
                                      const std::string& extension_id,
                                      const DictionaryValue* filter) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_dict = update.Get();
  DictionaryValue* filtered_events = NULL;
  if (!extension_dict->GetDictionary(kFilteredEvents, &filtered_events)) {
    filtered_events = new DictionaryValue;
    extension_dict->Set(kFilteredEvents, filtered_events);
  }
  ListValue* filter_list = NULL;
  if (!filtered_events->GetList(event_name, &filter_list)) {
    filter_list = new ListValue;
    filtered_events->SetWithoutPathExpansion(event_name, filter_list);
  }

  filter_list->Append(filter->DeepCopy());
}

void ExtensionPrefs::RemoveFilterFromEvent(const std::string& event_name,
                                           const std::string& extension_id,
                                           const DictionaryValue* filter) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_dict = update.Get();
  DictionaryValue* filtered_events = NULL;

  if (!extension_dict->GetDictionary(kFilteredEvents, &filtered_events))
    return;
  ListValue* filter_list = NULL;
  if (!filtered_events->GetListWithoutPathExpansion(event_name, &filter_list))
    return;

  for (size_t i = 0; i < filter_list->GetSize(); i++) {
    DictionaryValue* filter;
    CHECK(filter_list->GetDictionary(i, &filter));
    if (filter->Equals(filter)) {
      filter_list->Remove(i, NULL);
      break;
    }
  }
}

const DictionaryValue* ExtensionPrefs::GetFilteredEvents(
    const std::string& extension_id) const {
  const DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return NULL;
  const DictionaryValue* result = NULL;
  if (!extension->GetDictionary(kFilteredEvents, &result))
    return NULL;
  return result;
}

void ExtensionPrefs::SetRegisteredEvents(
    const std::string& extension_id, const std::set<std::string>& events) {
  ListValue* value = new ListValue();
  for (std::set<std::string>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    value->Append(new StringValue(*it));
  }
  UpdateExtensionPref(extension_id, kRegisteredEvents, value);
}

void ExtensionPrefs::SetExtensionRunning(const std::string& extension_id,
    bool is_running) {
  Value* value = Value::CreateBooleanValue(is_running);
  UpdateExtensionPref(extension_id, kPrefRunning, value);
}

bool ExtensionPrefs::IsExtensionRunning(const std::string& extension_id) {
  const DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  bool running = false;
  extension->GetBoolean(kPrefRunning, &running);
  return running;
}

void ExtensionPrefs::AddSavedFileEntry(
    const std::string& extension_id,
    const std::string& file_entry_id,
    const base::FilePath& file_path,
    bool writable) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_dict = update.Get();
  DictionaryValue* file_entries = NULL;
  if (!extension_dict->GetDictionary(kFileEntries, &file_entries)) {
    file_entries = new DictionaryValue();
    extension_dict->SetWithoutPathExpansion(kFileEntries, file_entries);
  }
  // Once a file's permissions are set they can't be changed.
  DictionaryValue* file_entry_dict = NULL;
  if (file_entries->GetDictionary(file_entry_id, &file_entry_dict))
    return;

  file_entry_dict = new DictionaryValue();
  file_entry_dict->SetString(kFileEntryPath, file_path.value());
  file_entry_dict->SetBoolean(kFileEntryWritable, writable);
  file_entries->SetWithoutPathExpansion(file_entry_id, file_entry_dict);
}

void ExtensionPrefs::ClearSavedFileEntries(
    const std::string& extension_id) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_dict = update.Get();
  extension_dict->Remove(kFileEntries, NULL);
}

void ExtensionPrefs::GetSavedFileEntries(
    const std::string& extension_id,
    std::vector<app_file_handler_util::SavedFileEntry>* out) {
  const DictionaryValue* prefs = GetExtensionPref(extension_id);
  const DictionaryValue* file_entries = NULL;
  if (!prefs->GetDictionary(kFileEntries, &file_entries))
    return;
  for (DictionaryValue::key_iterator it = file_entries->begin_keys();
       it != file_entries->end_keys(); ++it) {
    std::string id = *it;
    const DictionaryValue* file_entry = NULL;
    if (!file_entries->GetDictionaryWithoutPathExpansion(id, &file_entry))
      continue;
    base::FilePath::StringType path_string;
    if (!file_entry->GetString(kFileEntryPath, &path_string))
      continue;
    bool writable = false;
    if (!file_entry->GetBoolean(kFileEntryWritable, &writable))
      continue;
    base::FilePath file_path(path_string);
    out->push_back(app_file_handler_util::SavedFileEntry(
        id, file_path, writable));
  }
}

ExtensionOmniboxSuggestion
ExtensionPrefs::GetOmniboxDefaultSuggestion(const std::string& extension_id) {
  ExtensionOmniboxSuggestion suggestion;

  const DictionaryValue* extension = GetExtensionPref(extension_id);
  const DictionaryValue* dict = NULL;
  if (extension && extension->GetDictionary(kOmniboxDefaultSuggestion, &dict))
    suggestion.Populate(*dict, false);

  return suggestion;
}

void ExtensionPrefs::SetOmniboxDefaultSuggestion(
    const std::string& extension_id,
    const ExtensionOmniboxSuggestion& suggestion) {
  scoped_ptr<base::DictionaryValue> dict = suggestion.ToValue().Pass();
  UpdateExtensionPref(extension_id, kOmniboxDefaultSuggestion, dict.release());
}

bool ExtensionPrefs::IsIncognitoEnabled(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefIncognitoEnabled);
}

void ExtensionPrefs::SetIsIncognitoEnabled(const std::string& extension_id,
                                           bool enabled) {
  UpdateExtensionPref(extension_id, kPrefIncognitoEnabled,
                      Value::CreateBooleanValue(enabled));
}

bool ExtensionPrefs::AllowFileAccess(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefAllowFileAccess);
}

void ExtensionPrefs::SetAllowFileAccess(const std::string& extension_id,
                                        bool allow) {
  UpdateExtensionPref(extension_id, kPrefAllowFileAccess,
                      Value::CreateBooleanValue(allow));
}

bool ExtensionPrefs::HasAllowFileAccessSetting(
    const std::string& extension_id) const {
  const DictionaryValue* ext = GetExtensionPref(extension_id);
  return ext && ext->HasKey(kPrefAllowFileAccess);
}

ExtensionPrefs::LaunchType ExtensionPrefs::GetLaunchType(
    const Extension* extension,
    ExtensionPrefs::LaunchType default_pref_value) {
  int value = -1;
  LaunchType result = LAUNCH_REGULAR;

  if (ReadExtensionPrefInteger(extension->id(), kPrefLaunchType, &value) &&
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
    if (!extension->is_platform_app() && result == LAUNCH_WINDOW)
      result = LAUNCH_REGULAR;
  #endif

#if defined(OS_WIN)
    // We don't support app windows in Windows 8 single window Metro mode.
    if (win8::IsSingleWindowMetroMode() && result == LAUNCH_WINDOW)
      result = LAUNCH_REGULAR;
#endif  // OS_WIN

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
    // Apps with app.launch.container = 'panel' should always respect the
    // manifest setting.
    result = manifest_launch_container;
  } else if (manifest_launch_container == extension_misc::LAUNCH_TAB) {
    // Look for prefs that indicate the user's choice of launch
    // container.  The app's menu on the NTP provides a UI to set
    // this preference.  If no preference is set, |default_pref_value|
    // is used.
    ExtensionPrefs::LaunchType prefs_launch_type =
        GetLaunchType(extension, default_pref_value);

    if (prefs_launch_type == ExtensionPrefs::LAUNCH_WINDOW) {
      // If the pref is set to launch a window (or no pref is set, and
      // window opening is the default), make the container a window.
      result = extension_misc::LAUNCH_WINDOW;
#if defined(USE_ASH)
    } else if (prefs_launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN &&
               chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH) {
      // LAUNCH_FULLSCREEN launches in a maximized app window in ash.
      // For desktop chrome AURA on all platforms we should open the
      // application in full screen mode in the current tab, on the same
      // lines as non AURA chrome.
      result = extension_misc::LAUNCH_WINDOW;
#endif
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
}

namespace {

bool GetMediaGalleryPermissionFromDictionary(
    const DictionaryValue* dict,
    chrome::MediaGalleryPermission* out_permission) {
  std::string string_id;
  if (dict->GetString(kMediaGalleryIdKey, &string_id) &&
      base::StringToUint64(string_id, &out_permission->pref_id) &&
      dict->GetBoolean(kMediaGalleryHasPermissionKey,
                       &out_permission->has_permission)) {
    return true;
  }
  NOTREACHED();
  return false;
}

void RemoveMediaGalleryPermissionsFromExtension(
    PrefService* prefs,
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery_id) {
  ScopedExtensionPrefUpdate update(prefs, extension_id);
  DictionaryValue* extension_dict = update.Get();
  ListValue* permissions = NULL;
  if (!extension_dict->GetList(kMediaGalleriesPermissions, &permissions))
    return;

  for (ListValue::iterator it = permissions->begin();
       it != permissions->end();
       ++it) {
    const DictionaryValue* dict = NULL;
    if (!(*it)->GetAsDictionary(&dict))
      continue;
    chrome::MediaGalleryPermission perm;
    if (!GetMediaGalleryPermissionFromDictionary(dict, &perm))
      continue;
    if (perm.pref_id == gallery_id) {
      permissions->Erase(it, NULL);
      return;
    }
  }
}

}  // namespace

void ExtensionPrefs::SetMediaGalleryPermission(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery,
    bool has_access) {
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  DictionaryValue* extension_dict = update.Get();
  ListValue* permissions = NULL;
  if (!extension_dict->GetList(kMediaGalleriesPermissions, &permissions)) {
    permissions = new ListValue;
    extension_dict->Set(kMediaGalleriesPermissions, permissions);
  } else {
    // If the gallery is already in the list, update the permission.
    for (ListValue::const_iterator it = permissions->begin();
         it != permissions->end();
         ++it) {
      DictionaryValue* dict = NULL;
      if (!(*it)->GetAsDictionary(&dict))
        continue;
      chrome::MediaGalleryPermission perm;
      if (!GetMediaGalleryPermissionFromDictionary(dict, &perm))
        continue;
      if (perm.pref_id == gallery) {
        dict->SetBoolean(kMediaGalleryHasPermissionKey, has_access);
        return;
      }
    }
  }

  DictionaryValue* dict = new DictionaryValue;
  dict->SetString(kMediaGalleryIdKey, base::Uint64ToString(gallery));
  dict->SetBoolean(kMediaGalleryHasPermissionKey, has_access);
  permissions->Append(dict);
}

void ExtensionPrefs::UnsetMediaGalleryPermission(
    const std::string& extension_id,
    chrome::MediaGalleryPrefId gallery) {
  RemoveMediaGalleryPermissionsFromExtension(prefs_, extension_id, gallery);
}

std::vector<chrome::MediaGalleryPermission>
ExtensionPrefs::GetMediaGalleryPermissions(const std::string& extension_id) {
  std::vector<chrome::MediaGalleryPermission> result;
  const ListValue* permissions = NULL;
  if (ReadExtensionPrefList(extension_id, kMediaGalleriesPermissions,
                            &permissions)) {
    for (ListValue::const_iterator it = permissions->begin();
         it != permissions->end();
         ++it) {
      DictionaryValue* dict = NULL;
      if (!(*it)->GetAsDictionary(&dict))
        continue;
      chrome::MediaGalleryPermission perm;
      if (!GetMediaGalleryPermissionFromDictionary(dict, &perm))
        continue;
      result.push_back(perm);
    }
  }
  return result;
}

void ExtensionPrefs::RemoveMediaGalleryPermissions(
    chrome::MediaGalleryPrefId gallery_id) {
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions)
    return;

  for (DictionaryValue::key_iterator it = extensions->begin_keys();
       it != extensions->end_keys();
       ++it) {
    const std::string& id(*it);
    if (!Extension::IdIsValid(id)) {
      NOTREACHED();
      continue;
    }
    RemoveMediaGalleryPermissionsFromExtension(prefs_, id, gallery_id);
  }
}

bool ExtensionPrefs::DoesExtensionHaveState(
    const std::string& id, Extension::State check_state) const {
  const DictionaryValue* extension = GetExtensionPref(id);
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
  return GetExtensionPrefAsVector(GetToolbarOrderKeyName());
}

void ExtensionPrefs::SetToolbarOrder(const ExtensionIdList& extension_ids) {
  SetExtensionPrefFromVector(GetToolbarOrderKeyName(), extension_ids);
}

ExtensionIdList ExtensionPrefs::GetActionBoxOrder() {
  return GetExtensionPrefAsVector(kExtensionActionBox);
}

void ExtensionPrefs::SetActionBoxOrder(const ExtensionIdList& extension_ids) {
  SetExtensionPrefFromVector(kExtensionActionBox, extension_ids);
}

void ExtensionPrefs::OnExtensionInstalled(
    const Extension* extension,
    Extension::State initial_state,
    const syncer::StringOrdinal& page_ordinal) {
  ScopedExtensionPrefUpdate update(prefs_, extension->id());
  DictionaryValue* extension_dict = update.Get();
  const base::Time install_time = time_provider_->GetCurrentTime();
  PopulateExtensionInfoPrefs(extension, install_time, initial_state,
                             extension_dict);
  FinishExtensionInfoPrefs(extension->id(), install_time,
                           extension->RequiresSortOrdinal(),
                           page_ordinal, extension_dict);
}

void ExtensionPrefs::OnExtensionUninstalled(const std::string& extension_id,
                                            const Manifest::Location& location,
                                            bool external_uninstall) {
  extension_sorting_->ClearOrdinals(extension_id);

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall && Manifest::IsExternalLocation(location)) {
    UpdateExtensionPref(extension_id, kPrefState,
                        Value::CreateIntegerValue(
                            Extension::EXTERNAL_EXTENSION_UNINSTALLED));
    extension_pref_value_map_->SetExtensionState(extension_id, false);
    content_settings_store_->SetExtensionState(extension_id, false);
  } else {
    DeleteExtensionPrefs(extension_id);
  }
}

void ExtensionPrefs::SetExtensionState(const std::string& extension_id,
                                       Extension::State state) {
  UpdateExtensionPref(extension_id, kPrefState,
                      Value::CreateIntegerValue(state));
  bool enabled = (state == Extension::ENABLED);
  extension_pref_value_map_->SetExtensionState(extension_id, enabled);
  content_settings_store_->SetExtensionState(extension_id, enabled);
}

bool ExtensionPrefs::GetBrowserActionVisibility(const Extension* extension) {
  if (FeatureSwitch::extensions_in_action_box()->IsEnabled()) {
    ExtensionIdList ids = GetToolbarOrder();
    return find(ids.begin(), ids.end(), extension->id()) != ids.end();
  }

  const DictionaryValue* extension_prefs = GetExtensionPref(extension->id());
  if (!extension_prefs)
    return true;

  bool visible = false;
  if (!extension_prefs->GetBoolean(kBrowserActionVisible, &visible))
    return true;

  return visible;
}

void ExtensionPrefs::SetBrowserActionVisibility(const Extension* extension,
                                                bool visible) {
  if (GetBrowserActionVisibility(extension) == visible)
    return;

  if (FeatureSwitch::extensions_in_action_box()->IsEnabled()) {
    ExtensionIdList ids = GetToolbarOrder();
    ids.push_back(extension->id());
    SetToolbarOrder(ids);
  } else {
    UpdateExtensionPref(extension->id(), kBrowserActionVisible,
                        Value::CreateBooleanValue(visible));
  }
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_VISIBILITY_CHANGED,
      content::Source<ExtensionPrefs>(this),
      content::Details<const Extension>(extension));
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
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    const DictionaryValue* extension_dict = GetExtensionPref(extension->id());
    if (!extension_dict)
      return;
    const DictionaryValue* old_manifest = NULL;
    bool update_required =
        !extension_dict->GetDictionary(kPrefManifest, &old_manifest) ||
        !extension->manifest()->value()->Equals(old_manifest);
    if (update_required) {
      UpdateExtensionPref(extension->id(), kPrefManifest,
                          extension->manifest()->value()->DeepCopy());
    }
  }
}

base::FilePath ExtensionPrefs::GetExtensionPath(
    const std::string& extension_id) {
  const DictionaryValue* dict = GetExtensionPref(extension_id);
  if (!dict)
    return base::FilePath();

  std::string path;
  if (!dict->GetString(kPrefPath, &path))
    return base::FilePath();

  return install_directory_.Append(
      base::FilePath::FromWStringHack(UTF8ToWide(path)));
}

void ExtensionPrefs::UpdateExtensionPref(const std::string& extension_id,
                                         const std::string& key,
                                         Value* data_value) {
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
  content_settings_store_->UnregisterExtension(extension_id);
  DictionaryPrefUpdate update(prefs_, kExtensionsPref);
  DictionaryValue* dict = update.Get();
  dict->Remove(extension_id, NULL);
}

const DictionaryValue* ExtensionPrefs::GetExtensionPref(
    const std::string& extension_id) const {
  const DictionaryValue* dict = prefs_->GetDictionary(kExtensionsPref);
  if (!dict)
    return NULL;
  const DictionaryValue* extension = NULL;
  dict->GetDictionary(extension_id, &extension);
  return extension;
}

scoped_ptr<ExtensionInfo> ExtensionPrefs::GetInstalledExtensionInfo(
    const std::string& extension_id) const {
  const DictionaryValue* ext;
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
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
  base::FilePath::StringType path;
  int location_value;
  if (!ext->GetInteger(kPrefLocation, &location_value))
    return scoped_ptr<ExtensionInfo>();

  if (!ext->GetString(kPrefPath, &path))
    return scoped_ptr<ExtensionInfo>();

  // Make path absolute. Unpacked extensions will already have absolute paths,
  // otherwise make it so.
  Manifest::Location location =
      static_cast<Manifest::Location>(location_value);
  if (!Manifest::IsUnpackedLocation(location)) {
    DCHECK(location == Manifest::COMPONENT ||
           !base::FilePath(path).IsAbsolute());
    path = install_directory_.Append(path).value();
  }

  // Only the following extension types have data saved in the preferences.
  if (location != Manifest::INTERNAL &&
      !Manifest::IsUnpackedLocation(location) &&
      !Manifest::IsExternalLocation(location)) {
    NOTREACHED();
    return scoped_ptr<ExtensionInfo>();
  }

  const DictionaryValue* manifest = NULL;
  if (!Manifest::IsUnpackedLocation(location) &&
      !ext->GetDictionary(kPrefManifest, &manifest)) {
    LOG(WARNING) << "Missing manifest for extension " << extension_id;
    // Just a warning for now.
  }

  return scoped_ptr<ExtensionInfo>(
      new ExtensionInfo(manifest, extension_id, base::FilePath(path),
                        location));
}

scoped_ptr<ExtensionPrefs::ExtensionsInfo>
    ExtensionPrefs::GetInstalledExtensionsInfo() const {
    scoped_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  for (DictionaryValue::key_iterator extension_id = extensions->begin_keys();
       extension_id != extensions->end_keys(); ++extension_id) {
    if (!Extension::IdIsValid(*extension_id))
      continue;

    scoped_ptr<ExtensionInfo> info = GetInstalledExtensionInfo(*extension_id);
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info.release()));
  }

  return extensions_info.Pass();
}

void ExtensionPrefs::SetDelayedInstallInfo(
    const Extension* extension,
    Extension::State initial_state,
    const syncer::StringOrdinal& page_ordinal) {
  DictionaryValue* extension_dict = new DictionaryValue();
  PopulateExtensionInfoPrefs(extension, time_provider_->GetCurrentTime(),
                             initial_state, extension_dict);

  // Add transient data that is needed by FinishDelayedInstallInfo(), but
  // should not be in the final extension prefs. All entries here should have
  // a corresponding Remove() call in FinishDelayedInstallInfo().
  if (extension->RequiresSortOrdinal()) {
    extension_dict->SetString(
        kPrefSuggestedPageOrdinal,
        page_ordinal.IsValid() ? page_ordinal.ToInternalValue() : "");
  }

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
  DictionaryValue* extension_dict = update.Get();
  DictionaryValue* pending_install_dict = NULL;
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

  const base::Time install_time = time_provider_->GetCurrentTime();
  pending_install_dict->Set(
      kPrefInstallTime,
      Value::CreateStringValue(
          base::Int64ToString(install_time.ToInternalValue())));

  // Commit the delayed install data.
  extension_dict->MergeDictionary(pending_install_dict);
  FinishExtensionInfoPrefs(extension_id, install_time, needs_sort_ordinal,
                           suggested_page_ordinal, extension_dict);
  return true;
}

scoped_ptr<ExtensionInfo> ExtensionPrefs::GetDelayedInstallInfo(
    const std::string& extension_id) const {
  const DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return scoped_ptr<ExtensionInfo>();

  const DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kDelayedInstallInfo, &ext))
    return scoped_ptr<ExtensionInfo>();

  // TODO(mek): share code with GetInstalledExtensionInfo
  base::FilePath::StringType path;
  int location_value;
  if (!ext->GetInteger(kPrefLocation, &location_value))
    return scoped_ptr<ExtensionInfo>();

  if (!ext->GetString(kPrefPath, &path))
    return scoped_ptr<ExtensionInfo>();

  // Make path absolute. Unpacked extensions will already have absolute paths,
  // otherwise make it so.
  Manifest::Location location =
      static_cast<Manifest::Location>(location_value);
  if (!Manifest::IsUnpackedLocation(location)) {
    DCHECK(location == Manifest::COMPONENT ||
           !base::FilePath(path).IsAbsolute());
    path = install_directory_.Append(path).value();
  }

  // Only the following extension types have data saved in the preferences.
  if (location != Manifest::INTERNAL &&
      !Manifest::IsUnpackedLocation(location) &&
      !Manifest::IsExternalLocation(location)) {
    NOTREACHED();
    return scoped_ptr<ExtensionInfo>();
  }

  const DictionaryValue* manifest = NULL;
  if (!Manifest::IsUnpackedLocation(location) &&
      !ext->GetDictionary(kPrefManifest, &manifest)) {
    LOG(WARNING) << "Missing manifest for extension " << extension_id;
    // Just a warning for now.
  }

  return scoped_ptr<ExtensionInfo>(
      new ExtensionInfo(manifest, extension_id, base::FilePath(path),
                        location));
}

scoped_ptr<ExtensionPrefs::ExtensionsInfo> ExtensionPrefs::
    GetAllDelayedInstallInfo() const {
  scoped_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  for (DictionaryValue::key_iterator extension_id = extensions->begin_keys();
       extension_id != extensions->end_keys(); ++extension_id) {
    if (!Extension::IdIsValid(*extension_id))
      continue;

    scoped_ptr<ExtensionInfo> info = GetDelayedInstallInfo(*extension_id);
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info.release()));
  }

  return extensions_info.Pass();
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
}

bool ExtensionPrefs::GetSideloadWipeoutDone() const {
  return prefs_->GetBoolean(kSideloadWipeoutDone);
}

void ExtensionPrefs::SetSideloadWipeoutDone() {
  prefs_->SetBoolean(kSideloadWipeoutDone, true);
}

bool ExtensionPrefs::WasAppDraggedByUser(const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefUserDraggedApp);
}

void ExtensionPrefs::SetAppDraggedByUser(const std::string& extension_id) {
  UpdateExtensionPref(extension_id, kPrefUserDraggedApp,
                      Value::CreateBooleanValue(true));
}

void ExtensionPrefs::SetUpdateUrlData(const std::string& extension_id,
                                      const std::string& data) {
  UpdateExtensionPref(extension_id, kUpdateUrlData,
                      Value::CreateStringValue(data));
}

std::string ExtensionPrefs::GetUpdateUrlData(const std::string& extension_id) {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  if (!dictionary)
    return std::string();

  std::string data;
  dictionary->GetString(kUpdateUrlData, &data);
  return data;
}

void ExtensionPrefs::OnContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  if (incognito) {
    UpdateExtensionPref(
        extension_id, kPrefIncognitoContentSettings,
        content_settings_store_->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeIncognitoPersistent));
  } else {
    UpdateExtensionPref(
        extension_id, kPrefContentSettings,
        content_settings_store_->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeRegular));
  }
}

bool ExtensionPrefs::IsFromWebStore(
    const std::string& extension_id) const {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefFromWebStore, &result))
    return result;
  return false;
}

bool ExtensionPrefs::IsFromBookmark(
    const std::string& extension_id) const {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefFromBookmark, &result))
    return result;
  return false;
}

int ExtensionPrefs::GetCreationFlags(const std::string& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  if (!ReadExtensionPrefInteger(extension_id, kPrefCreationFlags,
                                &creation_flags)) {
    // Since kPrefCreationFlags was added later, it will be missing for
    // previously installed extensions.
    if (IsFromBookmark(extension_id))
      creation_flags |= Extension::FROM_BOOKMARK;
    if (IsFromWebStore(extension_id))
      creation_flags |= Extension::FROM_WEBSTORE;
    if (WasInstalledByDefault(extension_id))
      creation_flags |= Extension::WAS_INSTALLED_BY_DEFAULT;
  }
  return creation_flags;
}

bool ExtensionPrefs::WasInstalledByDefault(
    const std::string& extension_id) const {
  const DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary &&
      dictionary->GetBoolean(kPrefWasInstalledByDefault, &result))
    return result;
  return false;
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

  const base::DictionaryValue* extension_prefs;
  const base::Value* extension_prefs_value =
      pref_service->GetUserPrefValue(kExtensionsPref);
  if (!extension_prefs_value ||
      !extension_prefs_value->GetAsDictionary(&extension_prefs)) {
    return result;  // Empty set
  }

  for (base::DictionaryValue::key_iterator it = extension_prefs->begin_keys();
       it != extension_prefs->end_keys(); ++it) {
    const DictionaryValue* ext;
    if (!extension_prefs->GetDictionaryWithoutPathExpansion(*it, &ext)) {
      NOTREACHED() << "Invalid pref for extension " << *it;
      continue;
    }
    if (!IsBlacklistBitSet(ext))
      result.push_back(*it);
  }
  return result;
}

void ExtensionPrefs::FixMissingPrefs(const ExtensionIdList& extension_ids) {
  // Fix old entries that did not get an installation time entry when they
  // were installed or don't have a preferences field.
  for (ExtensionIdList::const_iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    if (GetInstallTime(*ext_id) == base::Time()) {
      LOG(INFO) << "Could not parse installation time of extension "
                << *ext_id << ". It was probably installed before setting "
                << kPrefInstallTime << " was introduced. Updating "
                << kPrefInstallTime << " to the current time.";
      const base::Time install_time = time_provider_->GetCurrentTime();
      UpdateExtensionPref(*ext_id, kPrefInstallTime, Value::CreateStringValue(
          base::Int64ToString(install_time.ToInternalValue())));
    }
  }
}

void ExtensionPrefs::LoadExtensionControlledPrefs(
    const std::string& extension_id,
    ExtensionPrefsScope scope) {
  std::string scope_string;
  bool success = ScopeToPrefKey(scope, &scope_string);
  DCHECK(success);
  std::string key = extension_id + "." + scope_string;
  const DictionaryValue* preferences = NULL;
  // First try the regular lookup.
  const DictionaryValue* source_dict = prefs_->GetDictionary(kExtensionsPref);
  if (!source_dict->GetDictionary(key, &preferences))
    return;

  for (DictionaryValue::Iterator i(*preferences); i.HasNext(); i.Advance()) {
    extension_pref_value_map_->SetExtensionPref(
        extension_id, i.key(), scope, i.value().DeepCopy());
  }
}

void ExtensionPrefs::InitPrefStore(bool extensions_disabled) {
  if (extensions_disabled) {
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
  extension_sorting_->Initialize(extension_ids);

  // Store extension controlled preference values in the
  // |extension_pref_value_map_|, which then informs the subscribers
  // (ExtensionPrefStores) about the winning values.
  for (ExtensionIdList::iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    extension_pref_value_map_->RegisterExtension(
        *ext_id,
        GetInstallTime(*ext_id),
        !IsExtensionDisabled(*ext_id));
    content_settings_store_->RegisterExtension(
        *ext_id,
        GetInstallTime(*ext_id),
        !IsExtensionDisabled(*ext_id));

    // Set regular extension controlled prefs.
    LoadExtensionControlledPrefs(*ext_id, kExtensionPrefsScopeRegular);
    // Set incognito extension controlled prefs.
    LoadExtensionControlledPrefs(*ext_id,
                                 kExtensionPrefsScopeIncognitoPersistent);
    // Set regular-only extension controlled prefs.
    LoadExtensionControlledPrefs(*ext_id, kExtensionPrefsScopeRegularOnly);

    // Set content settings.
    const DictionaryValue* extension_prefs = GetExtensionPref(*ext_id);
    DCHECK(extension_prefs);
    const ListValue* content_settings = NULL;
    if (extension_prefs->GetList(kPrefContentSettings,
                                 &content_settings)) {
      content_settings_store_->SetExtensionContentSettingFromList(
          *ext_id, content_settings,
          kExtensionPrefsScopeRegular);
    }
    if (extension_prefs->GetList(kPrefIncognitoContentSettings,
                                 &content_settings)) {
      content_settings_store_->SetExtensionContentSettingFromList(
          *ext_id, content_settings,
          kExtensionPrefsScopeIncognitoPersistent);
    }
  }

  extension_pref_value_map_->NotifyInitializationCompleted();
}

void ExtensionPrefs::SetExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    ExtensionPrefsScope scope,
    Value* value) {
#ifndef NDEBUG
  const PrefService::Preference* pref =
      pref_service()->FindPreference(pref_key.c_str());
  DCHECK(pref) << "Extension controlled preference key " << pref_key
               << " not registered.";
  DCHECK_EQ(pref->GetType(), value->GetType())
      << "Extension controlled preference " << pref_key << " has wrong type.";
#endif

  std::string scope_string;
  // ScopeToPrefKey() returns false if the scope is not persisted.
  if (ScopeToPrefKey(scope, &scope_string)) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ScopedExtensionControlledPrefUpdate update(prefs_, extension_id,
                                               scope_string);
    update->SetWithoutPathExpansion(pref_key, value->DeepCopy());
  }

  extension_pref_value_map_->SetExtensionPref(
      extension_id, pref_key, scope, value);
}

void ExtensionPrefs::RemoveExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    ExtensionPrefsScope scope) {
  DCHECK(pref_service()->FindPreference(pref_key.c_str()))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  std::string scope_string;
  // ScopeToPrefKey() returns false if the scope is not persisted.
  if (ScopeToPrefKey(scope, &scope_string)) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ScopedExtensionControlledPrefUpdate update(prefs_, extension_id,
                                               scope_string);
    update->RemoveWithoutPathExpansion(pref_key, NULL);
  }

  extension_pref_value_map_->RemoveExtensionPref(extension_id, pref_key, scope);
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
                                              bool* from_incognito) {
  DCHECK(pref_service()->FindPreference(pref_key.c_str()))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map_->DoesExtensionControlPref(extension_id,
                                                             pref_key,
                                                             from_incognito);
}

bool ExtensionPrefs::HasIncognitoPrefValue(const std::string& pref_key) {
  bool has_incognito_pref_value = false;
  extension_pref_value_map_->GetEffectivePrefValue(pref_key,
                                                   true,
                                                   &has_incognito_pref_value);
  return has_incognito_pref_value;
}

void ExtensionPrefs::ClearIncognitoSessionOnlyContentSettings() {
  ExtensionIdList extension_ids;
  GetExtensions(&extension_ids);
  for (ExtensionIdList::iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    content_settings_store_->ClearContentSettingsForExtension(
        *ext_id,
        kExtensionPrefsScopeIncognitoSessionOnly);
  }
}

URLPatternSet ExtensionPrefs::GetAllowedInstallSites() {
  URLPatternSet result;
  const ListValue* list = prefs_->GetList(prefs::kExtensionAllowedInstallSites);
  CHECK(list);

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string entry_string;
    URLPattern entry(URLPattern::SCHEME_ALL);
    if (!list->GetString(i, &entry_string) ||
        entry.Parse(entry_string) != URLPattern::PARSE_SUCCESS) {
      LOG(ERROR) << "Invalid value for preference: "
                 << prefs::kExtensionAllowedInstallSites
                 << "." << i;
      continue;
    }
    result.AddPattern(entry);
  }

  return result;
}

const base::DictionaryValue* ExtensionPrefs::GetGeometryCache(
    const std::string& extension_id) const {
  const DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return NULL;

  const DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kPrefGeometryCache, &ext))
    return NULL;

  return ext;
}

void ExtensionPrefs::SetGeometryCache(
    const std::string& extension_id,
    scoped_ptr<base::DictionaryValue> cache) {
  UpdateExtensionPref(extension_id, kPrefGeometryCache, cache.release());
}

ExtensionPrefs::ExtensionPrefs(
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    scoped_ptr<TimeProvider> time_provider)
    : prefs_(prefs),
      install_directory_(root_dir),
      extension_pref_value_map_(extension_pref_value_map),
      ALLOW_THIS_IN_INITIALIZER_LIST(extension_sorting_(
          new ExtensionSorting(this, prefs))),
      content_settings_store_(new ContentSettingsStore()),
      time_provider_(time_provider.Pass()) {
}

void ExtensionPrefs::Init(bool extensions_disabled) {
  MakePathsRelative();

  InitPrefStore(extensions_disabled);

  content_settings_store_->AddObserver(this);
}

void ExtensionPrefs::SetNeedsStorageGarbageCollection(bool value) {
  prefs_->SetBoolean(prefs::kExtensionStorageGarbageCollect, value);
}

bool ExtensionPrefs::NeedsStorageGarbageCollection() {
  return prefs_->GetBoolean(prefs::kExtensionStorageGarbageCollect);
}

// static
void ExtensionPrefs::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kExtensionsPref,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(kExtensionToolbar,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(kExtensionActionBox,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(kExtensionActionBoxBar,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kExtensionToolbarSize,
                                -1,  // default value
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(kExtensionsBlacklistUpdate,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kExtensionInstallAllowList,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kExtensionInstallDenyList,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(prefs::kExtensionInstallForceList,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kExtensionAllowedTypes,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(kWebStoreLogin,
                               std::string(),  // default value
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kExtensionBlacklistUpdateVersion,
                               "0",  // default value
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kExtensionStorageGarbageCollect,
                                false,  // default value
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(prefs::kLastExtensionsUpdateCheck,
                              0,  // default value
                              PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(prefs::kNextExtensionsUpdateCheck,
                              0,  // default value
                              PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kExtensionAllowedInstallSites,
                             PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(kExtensionsLastChromeVersion,
                               std::string(),  // default value
                               PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(kSideloadWipeoutDone,
                                false,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);

#if defined (TOOLKIT_VIEWS)
  registry->RegisterIntegerPref(prefs::kBrowserActionContainerWidth,
                                0,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

ExtensionIdList ExtensionPrefs::GetExtensionPrefAsVector(
    const char* pref) {
  ExtensionIdList extension_ids;
  const ListValue* list_of_values = prefs_->GetList(pref);
  if (!list_of_values)
    return extension_ids;

  std::string extension_id;
  for (size_t i = 0; i < list_of_values->GetSize(); ++i) {
    if (list_of_values->GetString(i, &extension_id))
      extension_ids.push_back(extension_id);
  }
  return extension_ids;
}

void ExtensionPrefs::SetExtensionPrefFromVector(
    const char* pref,
    const ExtensionIdList& strings) {
  ListPrefUpdate update(prefs_, pref);
  ListValue* list_of_values = update.Get();
  list_of_values->Clear();
  for (ExtensionIdList::const_iterator iter = strings.begin();
       iter != strings.end(); ++iter)
    list_of_values->Append(new StringValue(*iter));
}

void ExtensionPrefs::PopulateExtensionInfoPrefs(
    const Extension* extension,
    const base::Time install_time,
    Extension::State initial_state,
    DictionaryValue* extension_dict) {
  // Leave the state blank for component extensions so that old chrome versions
  // loading new profiles do not fail in GetInstalledExtensionInfo. Older
  // Chrome versions would only check for an omitted state.
  if (initial_state != Extension::ENABLED_COMPONENT)
    extension_dict->Set(kPrefState, Value::CreateIntegerValue(initial_state));

  extension_dict->Set(kPrefLocation,
                      Value::CreateIntegerValue(extension->location()));
  extension_dict->Set(kPrefCreationFlags,
                      Value::CreateIntegerValue(extension->creation_flags()));
  extension_dict->Set(kPrefFromWebStore,
                      Value::CreateBooleanValue(extension->from_webstore()));
  extension_dict->Set(kPrefFromBookmark,
                      Value::CreateBooleanValue(extension->from_bookmark()));
  extension_dict->Set(kPrefWasInstalledByDefault,
      Value::CreateBooleanValue(extension->was_installed_by_default()));
  extension_dict->Set(kPrefInstallTime,
                      Value::CreateStringValue(
                          base::Int64ToString(install_time.ToInternalValue())));

  base::FilePath::StringType path = MakePathRelative(install_directory_,
      extension->path());
  extension_dict->Set(kPrefPath, Value::CreateStringValue(path));
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    extension_dict->Set(kPrefManifest,
                        extension->manifest()->value()->DeepCopy());
  }
}

void ExtensionPrefs::FinishExtensionInfoPrefs(
    const std::string& extension_id,
    const base::Time install_time,
    bool needs_sort_ordinal,
    const syncer::StringOrdinal& suggested_page_ordinal,
    DictionaryValue* extension_dict) {
  // Reinitializes various preferences with empty dictionaries.
  extension_dict->Set(kPrefPreferences, new DictionaryValue());
  extension_dict->Set(kPrefIncognitoPreferences, new DictionaryValue());
  extension_dict->Set(kPrefRegularOnlyPreferences, new DictionaryValue());
  extension_dict->Set(kPrefContentSettings, new ListValue());
  extension_dict->Set(kPrefIncognitoContentSettings, new ListValue());

  // If this point has been reached, any pending installs should be considered
  // out of date.
  extension_dict->Remove(kDelayedInstallInfo, NULL);

  // Clear state that may be registered from a previous install.
  extension_dict->Remove(kRegisteredEvents, NULL);

  // FYI, all code below here races on sudden shutdown because
  // |extension_dict|, |extension_sorting_|, |extension_pref_value_map_|,
  // and |content_settings_store_| are updated non-transactionally. This is
  // probably not fixable without nested transactional updates to pref
  // dictionaries.
  if (needs_sort_ordinal) {
    extension_sorting_->EnsureValidOrdinals(extension_id,
                                            suggested_page_ordinal);
  }

  bool is_enabled = false;
  int initial_state;
  if (extension_dict->GetInteger(kPrefState, &initial_state)) {
    is_enabled = initial_state == Extension::ENABLED;
  }

  extension_pref_value_map_->RegisterExtension(extension_id, install_time,
                                               is_enabled);
  content_settings_store_->RegisterExtension(extension_id, install_time,
                                             is_enabled);
}

}  // namespace extensions
