// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_api.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/common/notification_service.h"

using base::IntToString;
namespace events = extension_event_names;

namespace {

const char kAppLaunchUrlKey[] = "appLaunchUrl";
const char kDescriptionKey[] = "description";
const char kEnabledKey[] = "enabled";
const char kHomepageURLKey[] = "homepageUrl";
const char kIconsKey[] = "icons";
const char kIdKey[] = "id";
const char kIsAppKey[] = "isApp";
const char kNameKey[] = "name";
const char kOfflineEnabledKey[] = "offlineEnabled";
const char kOptionsUrlKey[] = "optionsUrl";
const char kPermissionsKey[] = "permissions";
const char kMayDisableKey[] = "mayDisable";
const char kSizeKey[] = "size";
const char kUrlKey[] = "url";
const char kVersionKey[] = "version";

const char kExtensionCreateError[] =
    "Failed to create extension from manifest.";
const char kManifestParseError[] = "Failed to parse manifest.";
const char kNoExtensionError[] = "Failed to find extension with id *";
const char kNotAnAppError[] = "Extension * is not an App";
const char kUserCantDisableError[] = "Extension * can not be disabled by user";
}

ExtensionService* ExtensionManagementFunction::service() {
  return profile()->GetExtensionService();
}

static DictionaryValue* CreateExtensionInfo(const Extension& extension,
                                            bool enabled) {
  DictionaryValue* info = new DictionaryValue();
  info->SetString(kIdKey, extension.id());
  info->SetBoolean(kIsAppKey, extension.is_app());
  info->SetString(kNameKey, extension.name());
  info->SetBoolean(kEnabledKey, enabled);
  info->SetBoolean(kMayDisableKey,
                   Extension::UserMayDisable(extension.location()));
  info->SetBoolean(kOfflineEnabledKey, extension.offline_enabled());
  info->SetString(kVersionKey, extension.VersionString());
  info->SetString(kDescriptionKey, extension.description());
  info->SetString(kOptionsUrlKey,
                  extension.options_url().possibly_invalid_spec());
  info->SetString(kHomepageURLKey,
                  extension.GetHomepageURL().possibly_invalid_spec());
  if (extension.is_app())
    info->SetString(kAppLaunchUrlKey,
                    extension.GetFullLaunchURL().possibly_invalid_spec());

  const ExtensionIconSet::IconMap& icons = extension.icons().map();
  if (!icons.empty()) {
    ListValue* icon_list = new ListValue();
    std::map<int, std::string>::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      DictionaryValue* icon_info = new DictionaryValue();
      Extension::Icons size = static_cast<Extension::Icons>(icon_iter->first);
      GURL url = ExtensionIconSource::GetIconURL(
          &extension, size, ExtensionIconSet::MATCH_EXACTLY, false, NULL);
      icon_info->SetInteger(kSizeKey, icon_iter->first);
      icon_info->SetString(kUrlKey, url.spec());
      icon_list->Append(icon_info);
    }
    info->Set("icons", icon_list);
  }

  const std::set<std::string> perms =
      extension.GetActivePermissions()->GetAPIsAsStrings();
  ListValue* permission_list = new ListValue();
  if (!perms.empty()) {
    std::set<std::string>::const_iterator perms_iter;
    for (perms_iter = perms.begin(); perms_iter != perms.end(); ++perms_iter) {
      StringValue* permission_name = new StringValue(*perms_iter);
      permission_list->Append(permission_name);
    }
  }
  info->Set("permissions", permission_list);

  ListValue* host_permission_list = new ListValue();
  if (!extension.is_hosted_app()) {
    // Skip host permissions for hosted apps.
    const URLPatternSet host_perms =
        extension.GetActivePermissions()->explicit_hosts();
    if (!host_perms.is_empty()) {
      URLPatternSet::const_iterator host_perms_iter;
      for (host_perms_iter = host_perms.begin();
           host_perms_iter != host_perms.end();
           ++host_perms_iter) {
        StringValue* name = new StringValue(host_perms_iter->GetAsString());
        host_permission_list->Append(name);
      }
    }
  }
  info->Set("hostPermissions", host_permission_list);

  return info;
}

static void AddExtensionInfo(ListValue* list,
                             const ExtensionList& extensions,
                             bool enabled) {
  for (ExtensionList::const_iterator i = extensions.begin();
       i != extensions.end(); ++i) {
    const Extension& extension = **i;

    if (extension.location() == Extension::COMPONENT)
      continue;  // Skip built-in extensions.

    list->Append(CreateExtensionInfo(extension, enabled));
  }
}

bool GetAllExtensionsFunction::RunImpl() {
  ListValue* result = new ListValue();
  result_.reset(result);

  AddExtensionInfo(result, *service()->extensions(), true);
  AddExtensionInfo(result, *service()->disabled_extensions(), false);

  return true;
}

bool GetExtensionByIdFunction::RunImpl() {
  std::string extension_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  const Extension* extension = service()->GetExtensionById(extension_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNoExtensionError,
                                                     extension_id);
    return false;
  }
  bool enabled = service()->IsExtensionEnabled(extension_id);
  DictionaryValue* result = CreateExtensionInfo(*extension, enabled);
  result_.reset(result);

  return true;
}

bool GetPermissionWarningsByIdFunction::RunImpl() {
  std::string ext_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &ext_id));

  const Extension* extension = service()->GetExtensionById(ext_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNoExtensionError,
                                                       ext_id);
    return false;
  }

  ExtensionPermissionMessages warnings = extension->GetPermissionMessages();
  ListValue* result = new ListValue();
  for (ExtensionPermissionMessages::const_iterator i = warnings.begin();
       i < warnings.end(); ++i)
    result->Append(Value::CreateStringValue(i->message()));
  result_.reset(result);
  return true;
}

namespace {

// This class helps GetPermissionWarningsByManifestFunction manage
// sending manifest JSON strings to the utility process for parsing.
class SafeManifestJSONParser : public UtilityProcessHost::Client {
 public:
  SafeManifestJSONParser(GetPermissionWarningsByManifestFunction* client,
                 const std::string& manifest)
      : client_(client),
        manifest_(manifest) {}

  void Start() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(this, &SafeManifestJSONParser::StartWorkOnIOThread));
  }

  void StartWorkOnIOThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    utility_host_ = new UtilityProcessHost(this, BrowserThread::IO);
    utility_host_->Send(new ChromeUtilityMsg_ParseJSON(manifest_));
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(SafeManifestJSONParser, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                          OnJSONParseSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                          OnJSONParseFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnJSONParseSucceeded(const ListValue& wrapper) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    if (value->IsType(Value::TYPE_DICTIONARY))
      parsed_manifest_.reset(static_cast<DictionaryValue*>(value)->DeepCopy());
    else
      error_ = kManifestParseError;

    utility_host_ = NULL; // has already deleted itself
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &SafeManifestJSONParser::ReportResultFromUIThread));
  }

  void OnJSONParseFailed(const std::string& error) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    error_ = error;
    utility_host_ = NULL; // has already deleted itself
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(this,
                          &SafeManifestJSONParser::ReportResultFromUIThread));
  }

  void ReportResultFromUIThread() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (error_.empty() && parsed_manifest_.get())
      client_->OnParseSuccess(parsed_manifest_.release());
    else
      client_->OnParseFailure(error_);
  }

 private:
  ~SafeManifestJSONParser() {}

  // The client who we'll report results back to.
  GetPermissionWarningsByManifestFunction* client_;

  // Data to parse.
  std::string manifest_;

  // Results of parsing.
  scoped_ptr<DictionaryValue> parsed_manifest_;

  std::string error_;
  UtilityProcessHost* utility_host_;
};

}  // namespace

bool GetPermissionWarningsByManifestFunction::RunImpl() {
  std::string manifest_str;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &manifest_str));

  scoped_refptr<SafeManifestJSONParser> parser =
      new SafeManifestJSONParser(this, manifest_str);
  parser->Start();

  // Matched with a Release() in OnParseSuccess/Failure().
  AddRef();

  // Response is sent async in OnParseSuccess/Failure().
  return true;
}

void GetPermissionWarningsByManifestFunction::OnParseSuccess(
    DictionaryValue* parsed_manifest) {
  CHECK(parsed_manifest);

  scoped_refptr<Extension> extension = Extension::Create(
      FilePath(), Extension::INVALID, *parsed_manifest,
      Extension::STRICT_ERROR_CHECKS, &error_);
  if (!extension.get()) {
    OnParseFailure(kExtensionCreateError);
    return;
  }

  ExtensionPermissionMessages warnings = extension->GetPermissionMessages();
  ListValue* result = new ListValue();
  for (ExtensionPermissionMessages::const_iterator i = warnings.begin();
       i < warnings.end(); ++i)
    result->Append(Value::CreateStringValue(i->message()));
  result_.reset(result);
  SendResponse(true);

  // Matched with AddRef() in RunImpl().
  Release();
}

void GetPermissionWarningsByManifestFunction::OnParseFailure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);

  // Matched with AddRef() in RunImpl().
  Release();
}

bool LaunchAppFunction::RunImpl() {
  std::string extension_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  const Extension* extension = service()->GetExtensionById(extension_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNoExtensionError,
                                                     extension_id);
    return false;
  }
  if (!extension->is_app()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNotAnAppError,
                                                     extension_id);
    return false;
  }

  // Look at prefs to find the right launch container.
  // |default_pref_value| is set to LAUNCH_REGULAR so that if
  // the user has not set a preference, we open the app in a tab.
  extension_misc::LaunchContainer launch_container =
      service()->extension_prefs()->GetLaunchContainer(
          extension, ExtensionPrefs::LAUNCH_DEFAULT);
  Browser::OpenApplication(profile(), extension, launch_container,
                           NEW_FOREGROUND_TAB);
  UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                            extension_misc::APP_LAUNCH_EXTENSION_API,
                            extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  return true;
}

bool SetEnabledFunction::RunImpl() {
  std::string extension_id;
  bool enable;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &enable));

  const Extension* extension = service()->GetExtensionById(extension_id, true);
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoExtensionError, extension_id);
    return false;
  }

  if (!Extension::UserMayDisable(extension->location())) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kUserCantDisableError, extension_id);
    return false;
  }

  if (!service()->IsExtensionEnabled(extension_id) && enable)
    service()->EnableExtension(extension_id);
  else if (service()->IsExtensionEnabled(extension_id) && !enable)
    service()->DisableExtension(extension_id);

  return true;
}

bool UninstallFunction::RunImpl() {
  std::string extension_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_id));

  if (!service()->GetExtensionById(extension_id, true)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoExtensionError, extension_id);
    return false;
  }

  ExtensionPrefs* prefs = service()->extension_prefs();

  if (!Extension::UserMayDisable(
      prefs->GetInstalledExtensionInfo(extension_id)->extension_location)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kUserCantDisableError, extension_id);
    return false;
  }

  service()->UninstallExtension(extension_id, false /* external_uninstall */,
                                NULL);
  return true;
}

ExtensionManagementEventRouter::ExtensionManagementEventRouter(Profile* profile)
    : profile_(profile) {}

ExtensionManagementEventRouter::~ExtensionManagementEventRouter() {}

void ExtensionManagementEventRouter::Init() {
  int types[] = {
    chrome::NOTIFICATION_EXTENSION_INSTALLED,
    chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
    chrome::NOTIFICATION_EXTENSION_LOADED,
    chrome::NOTIFICATION_EXTENSION_UNLOADED
  };

  CHECK(registrar_.IsEmpty());
  for (size_t i = 0; i < arraysize(types); i++) {
    registrar_.Add(this,
                   types[i],
                   Source<Profile>(profile_));
  }
}

void ExtensionManagementEventRouter::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  const char* event_name = NULL;
  Profile* profile = Source<Profile>(source).ptr();
  CHECK(profile);
  CHECK(profile_->IsSameProfile(profile));

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      event_name = events::kOnExtensionInstalled;
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      event_name = events::kOnExtensionUninstalled;
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      event_name = events::kOnExtensionEnabled;
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      event_name = events::kOnExtensionDisabled;
      break;
    default:
      NOTREACHED();
      return;
  }

  ListValue args;
  if (event_name == events::kOnExtensionUninstalled) {
    const std::string& extension_id =
        Details<UninstalledExtensionInfo>(details).ptr()->extension_id;
    args.Append(Value::CreateStringValue(extension_id));
  } else {
    const Extension* extension = NULL;
    if (event_name == events::kOnExtensionDisabled) {
      extension = Details<UnloadedExtensionInfo>(details)->extension;
    } else {
      extension = Details<const Extension>(details).ptr();
    }
    CHECK(extension);
    ExtensionService* service = profile->GetExtensionService();
    bool enabled = service->GetExtensionById(extension->id(), false) != NULL;
    args.Append(CreateExtensionInfo(*extension, enabled));
  }

  std::string args_json;
  base::JSONWriter::Write(&args, false /* pretty_print */, &args_json);

  profile->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, args_json, NULL, GURL());
}
