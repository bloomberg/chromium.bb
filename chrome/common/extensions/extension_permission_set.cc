// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_permission_set.h"

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"

namespace ids = extension_misc;
namespace {

// Helper for GetDistinctHosts(): com > net > org > everything else.
bool RcdBetterThan(std::string a, std::string b) {
  if (a == b)
    return false;
  if (a == "com")
    return true;
  if (a == "net")
    return b != "com";
  if (a == "org")
    return b != "com" && b != "net";
  return false;
}

// Names of API modules that can be used without listing it in the
// permissions section of the manifest.
const char* kNonPermissionModuleNames[] = {
  "app",
  "browserAction",
  "devtools",
  "events",
  "extension",
  "i18n",
  "omnibox",
  "pageAction",
  "pageActions",
  "permissions",
  "runtime",
  "test",
  "types"
};
const size_t kNumNonPermissionModuleNames =
    arraysize(kNonPermissionModuleNames);

// Names of functions (within modules requiring permissions) that can be used
// without asking for the module permission. In other words, functions you can
// use with no permissions specified.
const char* kNonPermissionFunctionNames[] = {
  "management.getPermissionWarningsByManifest",
  "tabs.create",
  "tabs.onRemoved",
  "tabs.remove",
  "tabs.update",
};
const size_t kNumNonPermissionFunctionNames =
    arraysize(kNonPermissionFunctionNames);

const char kOldUnlimitedStoragePermission[] = "unlimited_storage";
const char kWindowsPermission[] = "windows";
const char kTemporaryBackgroundAlias[] = "background_alias_do_not_use";

void AddPatternsAndRemovePaths(const URLPatternSet& set, URLPatternSet* out) {
  DCHECK(out);
  for (URLPatternSet::const_iterator i = set.begin(); i != set.end(); ++i) {
    URLPattern p = *i;
    p.SetPath("/*");
    out->AddPattern(p);
  }
}

// Strips out the API name from a function or event name.
// Functions will be of the form api_name.function
// Events will be of the form api_name/id or api_name.optional.stuff
std::string GetPermissionName(const std::string& function_name) {
  size_t separator = function_name.find_first_of("./");
  if (separator != std::string::npos)
    return function_name.substr(0, separator);
  else
    return function_name;
}

}  // namespace

//
// PermissionMessage
//

// static
ExtensionPermissionMessage ExtensionPermissionMessage::CreateFromHostList(
    const std::set<std::string>& hosts) {
  std::vector<std::string> host_list(hosts.begin(), hosts.end());
  DCHECK_GT(host_list.size(), 0UL);
  ID message_id;
  string16 message;

  switch (host_list.size()) {
    case 1:
      message_id = kHosts1;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_1_HOST,
                                           UTF8ToUTF16(host_list[0]));
      break;
    case 2:
      message_id = kHosts2;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
                                           UTF8ToUTF16(host_list[0]),
                                           UTF8ToUTF16(host_list[1]));
      break;
    case 3:
      message_id = kHosts3;
      message = l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_3_HOSTS,
                                           UTF8ToUTF16(host_list[0]),
                                           UTF8ToUTF16(host_list[1]),
                                           UTF8ToUTF16(host_list[2]));
      break;
    default:
      message_id = kHosts4OrMore;
      message = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_WARNING_4_OR_MORE_HOSTS,
          UTF8ToUTF16(host_list[0]),
          UTF8ToUTF16(host_list[1]),
          base::IntToString16(hosts.size() - 2));
      break;
  }

  return ExtensionPermissionMessage(message_id, message);
}

ExtensionPermissionMessage::ExtensionPermissionMessage(
    ExtensionPermissionMessage::ID id, const string16& message)
  : id_(id), message_(message) {
}

ExtensionPermissionMessage::~ExtensionPermissionMessage() {}

//
// ExtensionPermission
//

ExtensionAPIPermission::~ExtensionAPIPermission() {}

ExtensionPermissionMessage ExtensionAPIPermission::GetMessage() const {
  return ExtensionPermissionMessage(
      message_id_, l10n_util::GetStringUTF16(l10n_message_id_));
}

ExtensionAPIPermission::ExtensionAPIPermission(
    ID id,
    const char* name,
    int l10n_message_id,
    ExtensionPermissionMessage::ID message_id,
    int flags)
    : id_(id),
      name_(name),
      flags_(flags),
      l10n_message_id_(l10n_message_id),
      message_id_(message_id) {}

// static
void ExtensionAPIPermission::RegisterAllPermissions(
    ExtensionPermissionsInfo* info) {

  struct PermissionRegistration {
    ExtensionAPIPermission::ID id;
    const char* name;
    int flags;
    int l10n_message_id;
    ExtensionPermissionMessage::ID message_id;
  } PermissionsToRegister[] = {
    // Register permissions for all extension types.
    { kBackground, "background" },
    { kClipboardRead, "clipboardRead", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
      ExtensionPermissionMessage::kClipboard },
    { kClipboardWrite, "clipboardWrite" },
    { kDeclarative, "declarative" },
    { kDeclarativeWebRequest, "declarativeWebRequest" },
    { kExperimental, "experimental", kFlagCannotBeOptional },
    { kGeolocation, "geolocation", kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
      ExtensionPermissionMessage::kGeolocation },
    { kNotification, "notifications" },
    { kUnlimitedStorage, "unlimitedStorage", kFlagCannotBeOptional },

    // Register hosted and packaged app permissions.
    { kAppNotifications, "appNotifications" },

    // Register extension permissions.
    { kActiveTab, "activeTab" },
    { kAlarms, "alarms" },
    { kAppWindow, "appWindow" },
    { kBookmark, "bookmarks", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
      ExtensionPermissionMessage::kBookmarks },
    { kBrowsingData, "browsingData" },
    { kContentSettings, "contentSettings", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
      ExtensionPermissionMessage::kContentSettings },
    { kContextMenus, "contextMenus" },
    { kCookie, "cookies" },
    { kFileBrowserHandler, "fileBrowserHandler", kFlagCannotBeOptional },
    { kFileSystem, "fileSystem" },
    { kHistory, "history", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      ExtensionPermissionMessage::kBrowsingHistory },
    { kKeybinding, "keybinding" },
    { kIdle, "idle" },
    { kInput, "input", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_INPUT,
      ExtensionPermissionMessage::kInput },
    { kManagement, "management", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
      ExtensionPermissionMessage::kManagement },
    { kPageCapture, "pageCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_ALL_PAGES_CONTENT,
      ExtensionPermissionMessage::kAllPageContent },
    { kPrivacy, "privacy", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_PRIVACY,
      ExtensionPermissionMessage::kPrivacy },
    { kStorage, "storage" },
    { kTab, "tabs", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS,
      ExtensionPermissionMessage::kTabs },
    { kTopSites, "topSites", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_BROWSING_HISTORY,
      ExtensionPermissionMessage::kBrowsingHistory },
    { kTts, "tts", 0, kFlagCannotBeOptional },
    { kTtsEngine, "ttsEngine", kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
      ExtensionPermissionMessage::kTtsEngine },
    { kUsb, "usb", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_USB,
      ExtensionPermissionMessage::kNone },
    { kWebNavigation, "webNavigation", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_TABS, ExtensionPermissionMessage::kTabs },
    { kWebRequest, "webRequest" },
    { kWebRequestBlocking, "webRequestBlocking" },

    // Register private permissions.
    { kChromeosInfoPrivate, "chromeosInfoPrivate", kFlagCannotBeOptional },
    { kFileBrowserHandlerInternal, "fileBrowserHandlerInternal",
      kFlagCannotBeOptional },
    { kFileBrowserPrivate, "fileBrowserPrivate", kFlagCannotBeOptional },
    { kManagedModePrivate, "managedModePrivate", kFlagCannotBeOptional },
    { kMediaPlayerPrivate, "mediaPlayerPrivate", kFlagCannotBeOptional },
    { kMetricsPrivate, "metricsPrivate", kFlagCannotBeOptional },
    { kSystemPrivate, "systemPrivate", kFlagCannotBeOptional },
    { kChromeAuthPrivate, "chromeAuthPrivate", kFlagCannotBeOptional },
    { kInputMethodPrivate, "inputMethodPrivate", kFlagCannotBeOptional },
    { kEchoPrivate, "echoPrivate", kFlagCannotBeOptional },
    { kTerminalPrivate, "terminalPrivate", kFlagCannotBeOptional },
    { kWebRequestInternal, "webRequestInternal", kFlagCannotBeOptional },
    { kWebSocketProxyPrivate, "webSocketProxyPrivate", kFlagCannotBeOptional },
    { kWebstorePrivate, "webstorePrivate", kFlagCannotBeOptional },

    // Full url access permissions.
    { kProxy, "proxy", kFlagImpliesFullURLAccess | kFlagCannotBeOptional },
    { kDebugger, "debugger", kFlagImpliesFullURLAccess | kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_DEBUGGER,
      ExtensionPermissionMessage::kDebugger },
    { kDevtools, "devtools",
      kFlagImpliesFullURLAccess | kFlagCannotBeOptional },
    { kPlugin, "plugin",
      kFlagImpliesFullURLAccess | kFlagImpliesFullAccess |
          kFlagCannotBeOptional,
      IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
      ExtensionPermissionMessage::kFullAccess },

    // Platform-app permissions.
    { kSocket, "socket", kFlagCannotBeOptional },
    { kAudioCapture, "audioCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
      ExtensionPermissionMessage::kAudioCapture },
    { kVideoCapture, "videoCapture", kFlagNone,
      IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
      ExtensionPermissionMessage::kVideoCapture },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(PermissionsToRegister); ++i) {
    const PermissionRegistration& pr = PermissionsToRegister[i];
    info->RegisterPermission(
        pr.id, pr.name, pr.l10n_message_id,
        pr.message_id ? pr.message_id : ExtensionPermissionMessage::kNone,
        pr.flags);
  }

  // Register aliases.
  info->RegisterAlias("unlimitedStorage", kOldUnlimitedStoragePermission);
  info->RegisterAlias("tabs", kWindowsPermission);
  // TODO(mihaip): Should be removed for the M20 branch, see
  // http://crbug.com/120447 for more details.
  info->RegisterAlias("background", kTemporaryBackgroundAlias);
}

//
// ExtensionPermissionsInfo
//

// static
ExtensionPermissionsInfo* ExtensionPermissionsInfo::GetInstance() {
  return Singleton<ExtensionPermissionsInfo>::get();
}

ExtensionAPIPermission* ExtensionPermissionsInfo::GetByID(
    ExtensionAPIPermission::ID id) {
  IDMap::iterator i = id_map_.find(id);
  return (i == id_map_.end()) ? NULL : i->second;
}

ExtensionAPIPermission* ExtensionPermissionsInfo::GetByName(
    const std::string& name) {
  NameMap::iterator i = name_map_.find(name);
  return (i == name_map_.end()) ? NULL : i->second;
}

ExtensionAPIPermissionSet ExtensionPermissionsInfo::GetAll() {
  ExtensionAPIPermissionSet permissions;
  for (IDMap::const_iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    permissions.insert(i->second->id());
  return permissions;
}

ExtensionAPIPermissionSet ExtensionPermissionsInfo::GetAllByName(
    const std::set<std::string>& permission_names) {
  ExtensionAPIPermissionSet permissions;
  for (std::set<std::string>::const_iterator i = permission_names.begin();
       i != permission_names.end(); ++i) {
    ExtensionAPIPermission* permission = GetByName(*i);
    if (permission)
      permissions.insert(permission->id());
  }
  return permissions;
}

ExtensionPermissionsInfo::~ExtensionPermissionsInfo() {
  for (IDMap::iterator i = id_map_.begin(); i != id_map_.end(); ++i)
    delete i->second;
}

ExtensionPermissionsInfo::ExtensionPermissionsInfo()
    : hosted_app_permission_count_(0),
      permission_count_(0) {
  ExtensionAPIPermission::RegisterAllPermissions(this);
}

void ExtensionPermissionsInfo::RegisterAlias(
    const char* name,
    const char* alias) {
  DCHECK(name_map_.find(name) != name_map_.end());
  DCHECK(name_map_.find(alias) == name_map_.end());
  name_map_[alias] = name_map_[name];
}

ExtensionAPIPermission* ExtensionPermissionsInfo::RegisterPermission(
    ExtensionAPIPermission::ID id,
    const char* name,
    int l10n_message_id,
    ExtensionPermissionMessage::ID message_id,
    int flags) {
  DCHECK(id_map_.find(id) == id_map_.end());
  DCHECK(name_map_.find(name) == name_map_.end());

  ExtensionAPIPermission* permission = new ExtensionAPIPermission(
      id, name, l10n_message_id, message_id, flags);

  id_map_[id] = permission;
  name_map_[name] = permission;

  permission_count_++;

  return permission;
}

//
// ExtensionPermissionSet
//

ExtensionPermissionSet::ExtensionPermissionSet() {}

ExtensionPermissionSet::ExtensionPermissionSet(
    const extensions::Extension* extension,
    const ExtensionAPIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const ExtensionOAuth2Scopes& scopes)
    : apis_(apis),
      scopes_(scopes) {
  DCHECK(extension);
  AddPatternsAndRemovePaths(explicit_hosts, &explicit_hosts_);
  InitImplicitExtensionPermissions(extension);
  InitEffectiveHosts();
}

ExtensionPermissionSet::ExtensionPermissionSet(
    const ExtensionAPIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts)
    : apis_(apis),
      scriptable_hosts_(scriptable_hosts) {
  AddPatternsAndRemovePaths(explicit_hosts, &explicit_hosts_);
  InitEffectiveHosts();
}

ExtensionPermissionSet::ExtensionPermissionSet(
    const ExtensionAPIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts,
    const ExtensionOAuth2Scopes& scopes)
    : apis_(apis),
      scriptable_hosts_(scriptable_hosts),
      scopes_(scopes) {
  AddPatternsAndRemovePaths(explicit_hosts, &explicit_hosts_);
  InitEffectiveHosts();
}

ExtensionPermissionSet::ExtensionPermissionSet(
    const ExtensionOAuth2Scopes& scopes)
    : scopes_(scopes) {
  InitEffectiveHosts();
}

// static
ExtensionPermissionSet* ExtensionPermissionSet::CreateDifference(
    const ExtensionPermissionSet* set1,
    const ExtensionPermissionSet* set2) {
  scoped_refptr<ExtensionPermissionSet> empty = new ExtensionPermissionSet();
  const ExtensionPermissionSet* set1_safe = (set1 == NULL) ? empty : set1;
  const ExtensionPermissionSet* set2_safe = (set2 == NULL) ? empty : set2;

  ExtensionAPIPermissionSet apis;
  std::set_difference(set1_safe->apis().begin(), set1_safe->apis().end(),
                      set2_safe->apis().begin(), set2_safe->apis().end(),
                      std::insert_iterator<ExtensionAPIPermissionSet>(
                          apis, apis.begin()));

  URLPatternSet explicit_hosts;
  URLPatternSet::CreateDifference(set1_safe->explicit_hosts(),
                                  set2_safe->explicit_hosts(),
                                  &explicit_hosts);

  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateDifference(set1_safe->scriptable_hosts(),
                                  set2_safe->scriptable_hosts(),
                                  &scriptable_hosts);

  ExtensionOAuth2Scopes scopes;
  std::set_difference(set1_safe->scopes().begin(), set1_safe->scopes().end(),
                      set2_safe->scopes().begin(), set2_safe->scopes().end(),
                      std::insert_iterator<ExtensionOAuth2Scopes>(
                          scopes, scopes.begin()));

  return new ExtensionPermissionSet(
      apis, explicit_hosts, scriptable_hosts, scopes);
}

// static
ExtensionPermissionSet* ExtensionPermissionSet::CreateIntersection(
    const ExtensionPermissionSet* set1,
    const ExtensionPermissionSet* set2) {
  scoped_refptr<ExtensionPermissionSet> empty = new ExtensionPermissionSet();
  const ExtensionPermissionSet* set1_safe = (set1 == NULL) ? empty : set1;
  const ExtensionPermissionSet* set2_safe = (set2 == NULL) ? empty : set2;

  ExtensionAPIPermissionSet apis;
  std::set_intersection(set1_safe->apis().begin(), set1_safe->apis().end(),
                        set2_safe->apis().begin(), set2_safe->apis().end(),
                        std::insert_iterator<ExtensionAPIPermissionSet>(
                            apis, apis.begin()));
  URLPatternSet explicit_hosts;
  URLPatternSet::CreateIntersection(set1_safe->explicit_hosts(),
                                    set2_safe->explicit_hosts(),
                                    &explicit_hosts);

  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateIntersection(set1_safe->scriptable_hosts(),
                                    set2_safe->scriptable_hosts(),
                                    &scriptable_hosts);

  ExtensionOAuth2Scopes scopes;
  std::set_intersection(set1_safe->scopes().begin(), set1_safe->scopes().end(),
                        set2_safe->scopes().begin(), set2_safe->scopes().end(),
                        std::insert_iterator<ExtensionOAuth2Scopes>(
                            scopes, scopes.begin()));

  return new ExtensionPermissionSet(
      apis, explicit_hosts, scriptable_hosts, scopes);
}

// static
ExtensionPermissionSet* ExtensionPermissionSet::CreateUnion(
    const ExtensionPermissionSet* set1,
    const ExtensionPermissionSet* set2) {
  scoped_refptr<ExtensionPermissionSet> empty = new ExtensionPermissionSet();
  const ExtensionPermissionSet* set1_safe = (set1 == NULL) ? empty : set1;
  const ExtensionPermissionSet* set2_safe = (set2 == NULL) ? empty : set2;

  ExtensionAPIPermissionSet apis;
  std::set_union(set1_safe->apis().begin(), set1_safe->apis().end(),
                 set2_safe->apis().begin(), set2_safe->apis().end(),
                 std::insert_iterator<ExtensionAPIPermissionSet>(
                     apis, apis.begin()));

  URLPatternSet explicit_hosts;
  URLPatternSet::CreateUnion(set1_safe->explicit_hosts(),
                             set2_safe->explicit_hosts(),
                             &explicit_hosts);

  URLPatternSet scriptable_hosts;
  URLPatternSet::CreateUnion(set1_safe->scriptable_hosts(),
                             set2_safe->scriptable_hosts(),
                             &scriptable_hosts);

  ExtensionOAuth2Scopes scopes;
  std::set_union(set1_safe->scopes().begin(), set1_safe->scopes().end(),
                 set2_safe->scopes().begin(), set2_safe->scopes().end(),
                 std::insert_iterator<ExtensionOAuth2Scopes>(
                     scopes, scopes.begin()));

  return new ExtensionPermissionSet(
      apis, explicit_hosts, scriptable_hosts, scopes);
}

bool ExtensionPermissionSet::operator==(
    const ExtensionPermissionSet& rhs) const {
  return apis_ == rhs.apis_ &&
      scriptable_hosts_ == rhs.scriptable_hosts_ &&
      explicit_hosts_ == rhs.explicit_hosts_ &&
      scopes_ == rhs.scopes_;
}

bool ExtensionPermissionSet::Contains(const ExtensionPermissionSet& set) const {
  // Every set includes the empty set.
  if (set.IsEmpty())
    return true;

  if (!std::includes(apis_.begin(), apis_.end(),
                     set.apis().begin(), set.apis().end()))
    return false;

  if (!explicit_hosts().Contains(set.explicit_hosts()))
    return false;

  if (!scriptable_hosts().Contains(set.scriptable_hosts()))
    return false;

  if (!std::includes(scopes_.begin(), scopes_.end(),
                     set.scopes().begin(), set.scopes().end()))
    return false;

  return true;
}

std::set<std::string> ExtensionPermissionSet::GetAPIsAsStrings() const {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  std::set<std::string> apis_str;
  for (ExtensionAPIPermissionSet::const_iterator i = apis_.begin();
       i != apis_.end(); ++i) {
    ExtensionAPIPermission* permission = info->GetByID(*i);
    if (permission)
      apis_str.insert(permission->name());
  }
  return apis_str;
}

std::set<std::string> ExtensionPermissionSet::
    GetAPIsWithAnyAccessAsStrings() const {
  std::set<std::string> result = GetAPIsAsStrings();
  for (size_t i = 0; i < kNumNonPermissionModuleNames; ++i)
    result.insert(kNonPermissionModuleNames[i]);
  for (size_t i = 0; i < kNumNonPermissionFunctionNames; ++i)
    result.insert(GetPermissionName(kNonPermissionFunctionNames[i]));
  return result;
}

bool ExtensionPermissionSet::HasAnyAccessToAPI(
    const std::string& api_name) const {
  if (HasAccessToFunction(api_name))
    return true;

  for (size_t i = 0; i < kNumNonPermissionFunctionNames; ++i) {
    if (api_name == GetPermissionName(kNonPermissionFunctionNames[i]))
      return true;
  }

  return false;
}

std::set<std::string>
    ExtensionPermissionSet::GetDistinctHostsForDisplay() const {
  return GetDistinctHosts(effective_hosts_, true, true);
}

ExtensionPermissionMessages
    ExtensionPermissionSet::GetPermissionMessages() const {
  ExtensionPermissionMessages messages;

  if (HasEffectiveFullAccess()) {
    messages.push_back(ExtensionPermissionMessage(
        ExtensionPermissionMessage::kFullAccess,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS)));
    return messages;
  }

  if (HasEffectiveAccessToAllHosts()) {
    messages.push_back(ExtensionPermissionMessage(
        ExtensionPermissionMessage::kHostsAll,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS)));
  } else {
    std::set<std::string> hosts = GetDistinctHostsForDisplay();
    if (!hosts.empty())
      messages.push_back(ExtensionPermissionMessage::CreateFromHostList(hosts));
  }

  std::set<ExtensionPermissionMessage> simple_msgs =
      GetSimplePermissionMessages();
  messages.insert(messages.end(), simple_msgs.begin(), simple_msgs.end());

  return messages;
}

std::vector<string16> ExtensionPermissionSet::GetWarningMessages() const {
  std::vector<string16> messages;
  ExtensionPermissionMessages permissions = GetPermissionMessages();

  bool audio_capture = false;
  bool video_capture = false;
  for (ExtensionPermissionMessages::const_iterator i = permissions.begin();
       i != permissions.end(); ++i) {
    if (i->id() == ExtensionPermissionMessage::kAudioCapture)
      audio_capture = true;
    if (i->id() == ExtensionPermissionMessage::kVideoCapture)
      video_capture = true;
  }

  for (ExtensionPermissionMessages::const_iterator i = permissions.begin();
       i != permissions.end(); ++i) {
    if (audio_capture && video_capture) {
      if (i->id() == ExtensionPermissionMessage::kAudioCapture) {
        messages.push_back(l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_AUDIO_AND_VIDEO_CAPTURE));
        continue;
      } else if (i->id() == ExtensionPermissionMessage::kVideoCapture) {
        // The combined message will be pushed above.
        continue;
      }
    }

    messages.push_back(i->message());
  }

  return messages;
}

bool ExtensionPermissionSet::IsEmpty() const {
  // Not default if any host permissions are present.
  if (!(explicit_hosts().is_empty() && scriptable_hosts().is_empty()))
    return false;

  // Or if it has no api permissions.
  return apis().empty();
}

bool ExtensionPermissionSet::HasAPIPermission(
    ExtensionAPIPermission::ID permission) const {
  return apis().find(permission) != apis().end();
}

bool ExtensionPermissionSet::HasAccessToFunction(
    const std::string& function_name) const {
  // TODO(jstritar): Embed this information in each permission and add a method
  // like GrantsAccess(function_name) to ExtensionAPIPermission. A "default"
  // permission can then handle the modules and functions that everyone can
  // access.
  for (size_t i = 0; i < kNumNonPermissionFunctionNames; ++i) {
    if (function_name == kNonPermissionFunctionNames[i])
      return true;
  }

  std::string permission_name = GetPermissionName(function_name);
  ExtensionAPIPermission* permission =
      ExtensionPermissionsInfo::GetInstance()->GetByName(permission_name);
  if (permission && apis_.count(permission->id()))
    return true;

  for (size_t i = 0; i < kNumNonPermissionModuleNames; ++i) {
    if (permission_name == kNonPermissionModuleNames[i]) {
      return true;
    }
  }

  return false;
}

bool ExtensionPermissionSet::HasExplicitAccessToOrigin(
    const GURL& origin) const {
  return explicit_hosts().MatchesURL(origin);
}

bool ExtensionPermissionSet::HasScriptableAccessToURL(
    const GURL& origin) const {
  // We only need to check our host list to verify access. The host list should
  // already reflect any special rules (such as chrome://favicon, all hosts
  // access, etc.).
  return scriptable_hosts().MatchesURL(origin);
}

bool ExtensionPermissionSet::HasEffectiveAccessToAllHosts() const {
  // There are two ways this set can have effective access to all hosts:
  //  1) it has an <all_urls> URL pattern.
  //  2) it has a named permission with implied full URL access.
  for (URLPatternSet::const_iterator host = effective_hosts().begin();
       host != effective_hosts().end(); ++host) {
    if (host->match_all_urls() ||
        (host->match_subdomains() && host->host().empty()))
      return true;
  }

  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  for (ExtensionAPIPermissionSet::const_iterator i = apis().begin();
       i != apis().end(); ++i) {
    ExtensionAPIPermission* permission = info->GetByID(*i);
    if (permission->implies_full_url_access())
      return true;
  }
  return false;
}

bool ExtensionPermissionSet::HasEffectiveAccessToURL(
    const GURL& url) const {
  return effective_hosts().MatchesURL(url);
}

bool ExtensionPermissionSet::HasEffectiveFullAccess() const {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  for (ExtensionAPIPermissionSet::const_iterator i = apis().begin();
       i != apis().end(); ++i) {
    ExtensionAPIPermission* permission = info->GetByID(*i);
    if (permission->implies_full_access())
      return true;
  }
  return false;
}

bool ExtensionPermissionSet::HasLessPrivilegesThan(
    const ExtensionPermissionSet* permissions) const {
  // Things can't get worse than native code access.
  if (HasEffectiveFullAccess())
    return false;

  // Otherwise, it's a privilege increase if the new one has full access.
  if (permissions->HasEffectiveFullAccess())
    return true;

  if (HasLessHostPrivilegesThan(permissions))
    return true;

  if (HasLessAPIPrivilegesThan(permissions))
    return true;

  if (HasLessScopesThan(permissions))
    return true;

  return false;
}

ExtensionPermissionSet::~ExtensionPermissionSet() {}

// static
std::set<std::string> ExtensionPermissionSet::GetDistinctHosts(
    const URLPatternSet& host_patterns,
    bool include_rcd,
    bool exclude_file_scheme) {
  // Use a vector to preserve order (also faster than a map on small sets).
  // Each item is a host split into two parts: host without RCDs and
  // current best RCD.
  typedef std::vector<std::pair<std::string, std::string> > HostVector;
  HostVector hosts_best_rcd;
  for (URLPatternSet::const_iterator i = host_patterns.begin();
       i != host_patterns.end(); ++i) {
    if (exclude_file_scheme && i->scheme() == chrome::kFileScheme)
      continue;

    std::string host = i->host();

    // Add the subdomain wildcard back to the host, if necessary.
    if (i->match_subdomains())
      host = "*." + host;

    // If the host has an RCD, split it off so we can detect duplicates.
    std::string rcd;
    size_t reg_len = net::RegistryControlledDomainService::GetRegistryLength(
        host, false);
    if (reg_len && reg_len != std::string::npos) {
      if (include_rcd)  // else leave rcd empty
        rcd = host.substr(host.size() - reg_len);
      host = host.substr(0, host.size() - reg_len);
    }

    // Check if we've already seen this host.
    HostVector::iterator it = hosts_best_rcd.begin();
    for (; it != hosts_best_rcd.end(); ++it) {
      if (it->first == host)
        break;
    }
    // If this host was found, replace the RCD if this one is better.
    if (it != hosts_best_rcd.end()) {
      if (include_rcd && RcdBetterThan(rcd, it->second))
        it->second = rcd;
    } else {  // Previously unseen host, append it.
      hosts_best_rcd.push_back(std::make_pair(host, rcd));
    }
  }

  // Build up the final vector by concatenating hosts and RCDs.
  std::set<std::string> distinct_hosts;
  for (HostVector::iterator it = hosts_best_rcd.begin();
       it != hosts_best_rcd.end(); ++it)
    distinct_hosts.insert(it->first + it->second);
  return distinct_hosts;
}

void ExtensionPermissionSet::InitImplicitExtensionPermissions(
    const extensions::Extension* extension) {
  // Add the implied permissions.
  if (!extension->plugins().empty())
    apis_.insert(ExtensionAPIPermission::kPlugin);

  if (!extension->devtools_url().is_empty())
    apis_.insert(ExtensionAPIPermission::kDevtools);

  // The webRequest permission implies the internal version as well.
  if (apis_.find(ExtensionAPIPermission::kWebRequest) != apis_.end())
    apis_.insert(ExtensionAPIPermission::kWebRequestInternal);

  // The fileBrowserHandler permission implies the internal version as well.
  if (apis_.find(ExtensionAPIPermission::kFileBrowserHandler) != apis_.end())
    apis_.insert(ExtensionAPIPermission::kFileBrowserHandlerInternal);

  // Add the scriptable hosts.
  for (UserScriptList::const_iterator content_script =
           extension->content_scripts().begin();
       content_script != extension->content_scripts().end(); ++content_script) {
    URLPatternSet::const_iterator pattern =
        content_script->url_patterns().begin();
    for (; pattern != content_script->url_patterns().end(); ++pattern)
      scriptable_hosts_.AddPattern(*pattern);
  }
}

void ExtensionPermissionSet::InitEffectiveHosts() {
  effective_hosts_.ClearPatterns();

  URLPatternSet::CreateUnion(
      explicit_hosts(), scriptable_hosts(), &effective_hosts_);
}

std::set<ExtensionPermissionMessage>
    ExtensionPermissionSet::GetSimplePermissionMessages() const {
  std::set<ExtensionPermissionMessage> messages;
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  for (ExtensionAPIPermissionSet::const_iterator i = apis_.begin();
       i != apis_.end(); ++i) {
    DCHECK_GT(ExtensionPermissionMessage::kNone,
              ExtensionPermissionMessage::kUnknown);
    ExtensionAPIPermission* perm = info->GetByID(*i);
    if (perm && perm->message_id() > ExtensionPermissionMessage::kNone)
      messages.insert(perm->GetMessage());
  }
  return messages;
}

bool ExtensionPermissionSet::HasLessAPIPrivilegesThan(
    const ExtensionPermissionSet* permissions) const {
  if (permissions == NULL)
    return false;

  std::set<ExtensionPermissionMessage> current_warnings =
      GetSimplePermissionMessages();
  std::set<ExtensionPermissionMessage> new_warnings =
      permissions->GetSimplePermissionMessages();
  std::set<ExtensionPermissionMessage> delta_warnings;
  std::set_difference(new_warnings.begin(), new_warnings.end(),
                      current_warnings.begin(), current_warnings.end(),
                      std::inserter(delta_warnings, delta_warnings.begin()));

  // We have less privileges if there are additional warnings present.
  return !delta_warnings.empty();
}

bool ExtensionPermissionSet::HasLessHostPrivilegesThan(
    const ExtensionPermissionSet* permissions) const {
  // If this permission set can access any host, then it can't be elevated.
  if (HasEffectiveAccessToAllHosts())
    return false;

  // Likewise, if the other permission set has full host access, then it must be
  // a privilege increase.
  if (permissions->HasEffectiveAccessToAllHosts())
    return true;

  const URLPatternSet& old_list = effective_hosts();
  const URLPatternSet& new_list = permissions->effective_hosts();

  // TODO(jstritar): This is overly conservative with respect to subdomains.
  // For example, going from *.google.com to www.google.com will be
  // considered an elevation, even though it is not (http://crbug.com/65337).
  std::set<std::string> new_hosts_set(GetDistinctHosts(new_list, false, false));
  std::set<std::string> old_hosts_set(GetDistinctHosts(old_list, false, false));
  std::set<std::string> new_hosts_only;

  std::set_difference(new_hosts_set.begin(), new_hosts_set.end(),
                      old_hosts_set.begin(), old_hosts_set.end(),
                      std::inserter(new_hosts_only, new_hosts_only.begin()));

  return !new_hosts_only.empty();
}

bool ExtensionPermissionSet::HasLessScopesThan(
    const ExtensionPermissionSet* permissions) const {
  if (permissions == NULL)
    return false;

  ExtensionOAuth2Scopes current_scopes = scopes();
  ExtensionOAuth2Scopes new_scopes = permissions->scopes();
  ExtensionOAuth2Scopes delta_scopes;
  std::set_difference(new_scopes.begin(), new_scopes.end(),
                      current_scopes.begin(), current_scopes.end(),
                      std::inserter(delta_scopes, delta_scopes.begin()));

  // We have less privileges if there are additional scopes present.
  return !delta_scopes.empty();
}
