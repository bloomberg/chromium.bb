// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_prefs.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/admin_policy.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/prefs/pref_notifier.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif  // OS_WIN

namespace extensions {

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

// Indicates whether an extension is blacklisted.
const char kPrefBlacklist[] = "blacklist";

// The oauth client id used for app notification setup.
const char kPrefAppNotificationClientId[] = "app_notif_client_id";

// Indicates whether the user has disabled notifications or not.
const char kPrefAppNotificationDisbaled[] = "app_notif_disabled";

// Indicates whether the user has acknowledged various types of extensions.
const char kPrefExternalAcknowledged[] = "ack_external";
const char kPrefBlacklistAcknowledged[] = "ack_blacklist";
const char kPrefOrphanAcknowledged[] = "ack_orphan";

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

// Provider of write access to a dictionary storing extension prefs.
class ScopedExtensionPrefUpdate : public DictionaryPrefUpdate {
 public:
  ScopedExtensionPrefUpdate(PrefService* service,
                            const std::string& extension_id) :
    DictionaryPrefUpdate(service, ExtensionPrefs::kExtensionsPref),
    extension_id_(extension_id) {}

  virtual ~ScopedExtensionPrefUpdate() {
  }

  virtual DictionaryValue* Get() {
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

  virtual DictionaryValue* Get() {
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
  return switch_utils::IsExtensionsInActionBoxEnabled() ?
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

ExtensionPrefs::ExtensionPrefs(
    PrefService* prefs,
    const FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map)
    : prefs_(prefs),
      install_directory_(root_dir),
      extension_pref_value_map_(extension_pref_value_map),
      ALLOW_THIS_IN_INITIALIZER_LIST(extension_sorting_(
          new ExtensionSorting(this, prefs))),
      content_settings_store_(new ContentSettingsStore()) {
}

ExtensionPrefs::~ExtensionPrefs() {
}

void ExtensionPrefs::Init(bool extensions_disabled) {
  MakePathsRelative();

  InitPrefStore(extensions_disabled);

  content_settings_store_->AddObserver(this);
}

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
    const DictionaryValue* extension_dict = NULL;
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
  DictionaryValue* update_dict = update.Get();
  for (std::set<std::string>::iterator i = absolute_keys.begin();
       i != absolute_keys.end(); ++i) {
    DictionaryValue* extension_dict = NULL;
    if (!update_dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict)) {
      NOTREACHED() << "Control should never reach here for extension " << *i;
      continue;
    }
    FilePath::StringType path_string;
    extension_dict->GetString(kPrefPath, &path_string);
    FilePath path(path_string);
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
    PermissionsInfo* info = PermissionsInfo::GetInstance();
    for (size_t i = 0; i < api_values->GetSize(); ++i) {
      const DictionaryValue* permission_dict = NULL;
      std::string permission_name;
      if (!api_values->GetString(i, &permission_name) &&
          !api_values->GetDictionary(i, &permission_dict)) {
        NOTREACHED() << "Permission is not a string or dict. ";
        continue;
      }

      const base::Value *permission_detail = NULL;
      if (permission_dict) {
        if (permission_dict->size() != 1u) {
          NOTREACHED() << "Permission is not a single key dict.";
          continue;
        }
        base::DictionaryValue::Iterator it(*permission_dict);
        permission_name = it.key();
        permission_detail = &it.value();
      }

      const APIPermissionInfo *permission_info =
        info->GetByName(permission_name);
      if (!permission_info) {
        NOTREACHED() << "Unknown permission[" << permission_name << "].";
        continue;
      }

      scoped_ptr<APIPermission> permission(
          permission_info->CreateAPIPermission());
      if (!permission->FromValue(permission_detail)) {
        NOTREACHED() << "Parse permission failed.";
        continue;
      }
      apis.insert(permission.release());
    }
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
    Value* detail = NULL;
    i->ToValue(&detail);
    if (detail) {
      DictionaryValue* tmp = new DictionaryValue();
      tmp->Set(i->name(), detail);
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

bool ExtensionPrefs::IsExtensionOrphaned(const std::string& extension_id) {
  // TODO(miket): we believe that this test will hinge on the number of
  // consecutive times that an update check has returned a certain response
  // versus a success response. For now nobody is orphaned.
  return false;
}

bool ExtensionPrefs::IsExternalExtensionAcknowledged(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefExternalAcknowledged);
}

void ExtensionPrefs::AcknowledgeExternalExtension(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalAcknowledged,
                      Value::CreateBooleanValue(true));
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
}

bool ExtensionPrefs::IsOrphanedExtensionAcknowledged(
    const std::string& extension_id) {
  return ReadExtensionPrefBoolean(extension_id, kPrefOrphanAcknowledged);
}

void ExtensionPrefs::AcknowledgeOrphanedExtension(
    const std::string& extension_id) {
  DCHECK(Extension::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefOrphanAcknowledged,
                      Value::CreateBooleanValue(true));
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

std::string ExtensionPrefs::GetDebugPolicyProviderName() const {
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "admin policy black/white/forcelist, via the ExtensionPrefs";
#endif
}

bool ExtensionPrefs::UserMayLoad(const Extension* extension,
                                 string16* error) const {
  const DictionaryValue* ext_prefs = GetExtensionPref(extension->id());
  bool is_google_blacklisted = ext_prefs && IsBlacklistBitSet(ext_prefs);

  const base::ListValue* blacklist =
      prefs_->GetList(prefs::kExtensionInstallDenyList);
  const base::ListValue* whitelist =
      prefs_->GetList(prefs::kExtensionInstallAllowList);
  const base::ListValue* forcelist =
      prefs_->GetList(prefs::kExtensionInstallForceList);
  return admin_policy::UserMayLoad(is_google_blacklisted, blacklist, whitelist,
                                   forcelist, extension, error);
}

bool ExtensionPrefs::UserMayModifySettings(const Extension* extension,
                                           string16* error) const {
  return admin_policy::UserMayModifySettings(extension, error);
}

bool ExtensionPrefs::MustRemainEnabled(const Extension* extension,
                                       string16* error) const {
  return admin_policy::MustRemainEnabled(extension, error);
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

void ExtensionPrefs::UpdateBlacklist(
    const std::set<std::string>& blacklist_set) {
  ExtensionIds remove_pref_ids;
  std::set<std::string> used_id_set;
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);

  if (extensions) {
    for (DictionaryValue::key_iterator extension_id = extensions->begin_keys();
         extension_id != extensions->end_keys(); ++extension_id) {
      const DictionaryValue* ext;
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
            UpdateExtensionPref(id, kPrefBlacklist, NULL);
          }
        }
      } else {
        if (!IsBlacklistBitSet(ext)) {
          // Only set the blacklist if it was not set.
          UpdateExtensionPref(id, kPrefBlacklist,
                              Value::CreateBooleanValue(true));
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
  for (size_t i = 0; i < remove_pref_ids.size(); ++i) {
    DeleteExtensionPrefs(remove_pref_ids[i]);
  }
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

void ExtensionPrefs::MigratePermissions(const ExtensionIds& extension_ids) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  for (ExtensionIds::const_iterator ext_id =
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
    const ExtensionIds& extension_ids) {
  for (ExtensionIds::const_iterator ext_id =
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

#if defined(OS_WIN)
    // We don't support app windows in windows 8 metro mode.
    if (base::win::IsMetroProcess() && result == LAUNCH_WINDOW)
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
        GetLaunchType(extension->id(), default_pref_value);

    if (prefs_launch_type == ExtensionPrefs::LAUNCH_WINDOW) {
      // If the pref is set to launch a window (or no pref is set, and
      // window opening is the default), make the container a window.
      result = extension_misc::LAUNCH_WINDOW;
#if defined(USE_ASH)
    } else if (prefs_launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN) {
      // LAUNCH_FULLSCREEN launches in a maximized app window in ash.
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

ExtensionPrefs::ExtensionIds ExtensionPrefs::GetToolbarOrder() {
  return GetExtensionPrefAsVector(GetToolbarOrderKeyName());
}

void ExtensionPrefs::SetToolbarOrder(const ExtensionIds& extension_ids) {
  SetExtensionPrefFromVector(GetToolbarOrderKeyName(), extension_ids);
}

ExtensionPrefs::ExtensionIds ExtensionPrefs::GetActionBoxOrder() {
  return GetExtensionPrefAsVector(kExtensionActionBox);
}

void ExtensionPrefs::SetActionBoxOrder(const ExtensionIds& extension_ids) {
  SetExtensionPrefFromVector(kExtensionActionBox, extension_ids);
}

void ExtensionPrefs::OnExtensionInstalled(
    const Extension* extension,
    Extension::State initial_state,
    bool from_webstore,
    const syncer::StringOrdinal& page_ordinal) {
  const std::string& id = extension->id();
  CHECK(Extension::IdIsValid(id));
  ScopedExtensionPrefUpdate update(prefs_, id);
  DictionaryValue* extension_dict = update.Get();
  const base::Time install_time = GetCurrentTime();
  extension_dict->Set(kPrefState, Value::CreateIntegerValue(initial_state));
  extension_dict->Set(kPrefLocation,
                      Value::CreateIntegerValue(extension->location()));
  extension_dict->Set(kPrefFromWebStore,
                      Value::CreateBooleanValue(from_webstore));
  extension_dict->Set(kPrefFromBookmark,
                      Value::CreateBooleanValue(extension->from_bookmark()));
  extension_dict->Set(kPrefWasInstalledByDefault,
      Value::CreateBooleanValue(extension->was_installed_by_default()));
  extension_dict->Set(kPrefInstallTime,
                      Value::CreateStringValue(
                          base::Int64ToString(install_time.ToInternalValue())));
  extension_dict->Set(kPrefPreferences, new DictionaryValue());
  extension_dict->Set(kPrefIncognitoPreferences, new DictionaryValue());
  extension_dict->Set(kPrefRegularOnlyPreferences, new DictionaryValue());
  extension_dict->Set(kPrefContentSettings, new ListValue());
  extension_dict->Set(kPrefIncognitoContentSettings, new ListValue());

  FilePath::StringType path = MakePathRelative(install_directory_,
      extension->path());
  extension_dict->Set(kPrefPath, Value::CreateStringValue(path));
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (extension->location() != Extension::LOAD) {
    extension_dict->Set(kPrefManifest,
                        extension->manifest()->value()->DeepCopy());
  }

  // Clear state that may be registered from a previous install.
  extension_dict->Remove(kRegisteredEvents, NULL);

  if (extension->is_app()) {
    syncer::StringOrdinal new_page_ordinal = page_ordinal.IsValid() ?
        page_ordinal : extension_sorting_->GetNaturalAppPageOrdinal();
    if (!extension_sorting_->GetPageOrdinal(id).IsValid())
      extension_sorting_->SetPageOrdinal(id, new_page_ordinal);
    if (!extension_sorting_->GetAppLaunchOrdinal(id).IsValid())
      extension_sorting_->SetAppLaunchOrdinal(
          id, extension_sorting_->CreateNextAppLaunchOrdinal(new_page_ordinal));
  }

  extension_pref_value_map_->RegisterExtension(
      id, install_time, initial_state == Extension::ENABLED);
  content_settings_store_->RegisterExtension(
      id, install_time, initial_state == Extension::ENABLED);
}

void ExtensionPrefs::OnExtensionUninstalled(const std::string& extension_id,
                                            const Extension::Location& location,
                                            bool external_uninstall) {
  extension_sorting_->ClearOrdinals(extension_id);

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall && Extension::IsExternalLocation(location)) {
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
  if (switch_utils::IsExtensionsInActionBoxEnabled()) {
    ExtensionIds ids = GetToolbarOrder();
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

  if (switch_utils::IsExtensionsInActionBoxEnabled()) {
    ExtensionIds ids = GetToolbarOrder();
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
  if (extension->location() != Extension::LOAD) {
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

FilePath ExtensionPrefs::GetExtensionPath(const std::string& extension_id) {
  const DictionaryValue* dict = GetExtensionPref(extension_id);
  if (!dict)
    return FilePath();

  std::string path;
  if (!dict->GetString(kPrefPath, &path))
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

ExtensionInfo* ExtensionPrefs::GetInstalledExtensionInfo(
    const std::string& extension_id) const {
  const DictionaryValue* ext;
  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  if (!extensions ||
      !extensions->GetDictionaryWithoutPathExpansion(extension_id, &ext))
    return NULL;
  if (IsBlacklistBitSet(ext))
      return NULL;
  int state_value;
  if (!ext->GetInteger(kPrefState, &state_value)) {
    // This can legitimately happen if we store preferences for component
    // extensions.
    return NULL;
  }
  if (state_value == Extension::EXTERNAL_EXTENSION_UNINSTALLED) {
    LOG(WARNING) << "External extension with id " << extension_id
                 << " has been uninstalled by the user";
    return NULL;
  }
  FilePath::StringType path;
  int location_value;
  if (!ext->GetInteger(kPrefLocation, &location_value))
    return NULL;

  if (!ext->GetString(kPrefPath, &path))
    return NULL;

  // Make path absolute. Unpacked extensions will already have absolute paths,
  // otherwise make it so.
  if (location_value != Extension::LOAD) {
    DCHECK(location_value == Extension::COMPONENT ||
           !FilePath(path).IsAbsolute());
    path = install_directory_.Append(path).value();
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

  const DictionaryValue* manifest = NULL;
  if (location != Extension::LOAD &&
      !ext->GetDictionary(kPrefManifest, &manifest)) {
    LOG(WARNING) << "Missing manifest for extension " << extension_id;
    // Just a warning for now.
  }

  return new ExtensionInfo(manifest, extension_id, FilePath(path), location);
}

ExtensionPrefs::ExtensionsInfo* ExtensionPrefs::GetInstalledExtensionsInfo(
    ) const {
  ExtensionsInfo* extensions_info = new ExtensionsInfo;

  const DictionaryValue* extensions = prefs_->GetDictionary(kExtensionsPref);
  for (DictionaryValue::key_iterator extension_id = extensions->begin_keys();
       extension_id != extensions->end_keys(); ++extension_id) {
    if (!Extension::IdIsValid(*extension_id))
      continue;

    ExtensionInfo* info = GetInstalledExtensionInfo(*extension_id);
    if (info)
      extensions_info->push_back(linked_ptr<ExtensionInfo>(info));
  }

  return extensions_info;
}

void ExtensionPrefs::SetIdleInstallInfo(const std::string& extension_id,
                                        const FilePath& crx_path,
                                        const std::string& version,
                                        const base::Time& fetch_time) {
  DictionaryValue* info = new DictionaryValue();
  info->SetString(kIdleInstallInfoCrxPath, crx_path.value());
  info->SetString(kIdleInstallInfoVersion, version);
  info->SetString(kIdleInstallInfoFetchTime,
                  base::Int64ToString(fetch_time.ToInternalValue()));
  UpdateExtensionPref(extension_id, kIdleInstallInfo, info);
}

bool ExtensionPrefs::RemoveIdleInstallInfo(const std::string& extension_id) {
  if (!GetExtensionPref(extension_id))
    return false;
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  bool result = update->Remove(kIdleInstallInfo, NULL);
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
  const DictionaryValue* info = NULL;
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

base::Time ExtensionPrefs::GetCurrentTime() const {
  return base::Time::Now();
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

void ExtensionPrefs::GetExtensions(ExtensionIds* out) {
  CHECK(out);

  scoped_ptr<ExtensionsInfo> extensions_info(GetInstalledExtensionsInfo());

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    out->push_back(info->extension_id);
  }
}

// static
ExtensionPrefs::ExtensionIds ExtensionPrefs::GetExtensionsFrom(
    const PrefService* pref_service) {
  ExtensionIds result;

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

void ExtensionPrefs::FixMissingPrefs(const ExtensionIds& extension_ids) {
  // Fix old entries that did not get an installation time entry when they
  // were installed or don't have a preferences field.
  for (ExtensionIds::const_iterator ext_id = extension_ids.begin();
       ext_id != extension_ids.end(); ++ext_id) {
    if (GetInstallTime(*ext_id) == base::Time()) {
      LOG(INFO) << "Could not parse installation time of extension "
                << *ext_id << ". It was probably installed before setting "
                << kPrefInstallTime << " was introduced. Updating "
                << kPrefInstallTime << " to the current time.";
      const base::Time install_time = GetCurrentTime();
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
  ExtensionIds extension_ids;
  GetExtensions(&extension_ids);
  // Create empty preferences dictionary for each extension (these dictionaries
  // are pruned when persisting the preferences to disk).
  for (ExtensionIds::iterator ext_id = extension_ids.begin();
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
  for (ExtensionIds::iterator ext_id = extension_ids.begin();
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
  ExtensionIds extension_ids;
  GetExtensions(&extension_ids);
  for (ExtensionIds::iterator ext_id = extension_ids.begin();
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

// static
void ExtensionPrefs::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(kExtensionsPref, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kExtensionToolbar, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kExtensionActionBox, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kExtensionActionBoxBar, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kExtensionToolbarSize,
                             -1,  // default value
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(kExtensionsBlacklistUpdate,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kExtensionInstallAllowList,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kExtensionInstallDenyList,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kExtensionInstallForceList,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(kWebStoreLogin,
                            std::string(),  // default value
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kExtensionBlacklistUpdateVersion,
                            "0",  // default value
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterInt64Pref(prefs::kLastExtensionsUpdateCheck,
                           0,  // default value
                           PrefService::UNSYNCABLE_PREF);
  prefs->RegisterInt64Pref(prefs::kNextExtensionsUpdateCheck,
                           0,  // default value
                           PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kExtensionAllowedInstallSites,
                          PrefService::UNSYNCABLE_PREF);
}

ExtensionPrefs::ExtensionIds ExtensionPrefs::GetExtensionPrefAsVector(
    const char* pref) {
  ExtensionIds extension_ids;
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

void ExtensionPrefs::SetExtensionPrefFromVector(const char* pref,
                                                const ExtensionIds& strings) {
  ListPrefUpdate update(prefs_, pref);
  ListValue* list_of_values = update.Get();
  list_of_values->Clear();
  for (ExtensionIds::const_iterator iter = strings.begin();
       iter != strings.end(); ++iter)
    list_of_values->Append(new StringValue(*iter));
}

}  // namespace extensions
