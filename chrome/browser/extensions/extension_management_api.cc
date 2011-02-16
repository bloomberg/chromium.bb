// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

using base::IntToString;
namespace events = extension_event_names;

namespace {

const char kAppLaunchUrlKey[] = "appLaunchUrl";
const char kDescriptionKey[] = "description";
const char kEnabledKey[] = "enabled";
const char kIconsKey[] = "icons";
const char kIdKey[] = "id";
const char kIsAppKey[] = "isApp";
const char kNameKey[] = "name";
const char kOptionsUrlKey[] = "optionsUrl";
const char kPermissionsKey[] = "permissions";
const char kSizeKey[] = "size";
const char kUrlKey[] = "url";
const char kVersionKey[] = "version";

const char kNoExtensionError[] = "No extension with id *";
const char kNotAnAppError[] = "Extension * is not an App";
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
  info->SetString(kVersionKey, extension.VersionString());
  info->SetString(kDescriptionKey, extension.description());
  info->SetString(kOptionsUrlKey,
                    extension.options_url().possibly_invalid_spec());
  if (extension.is_app())
    info->SetString(kAppLaunchUrlKey,
                    extension.GetFullLaunchURL().possibly_invalid_spec());

  const ExtensionIconSet::IconMap& icons = extension.icons().map();
  if (!icons.empty()) {
    ListValue* icon_list = new ListValue();
    std::map<int, std::string>::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      DictionaryValue* icon_info = new DictionaryValue();
      GURL url = extension.GetResourceURL(icon_iter->second);
      icon_info->SetInteger(kSizeKey, icon_iter->first);
      icon_info->SetString(kUrlKey, url.possibly_invalid_spec());
      icon_list->Append(icon_info);
    }
    info->Set("icons", icon_list);
  }

  const std::set<std::string> perms = extension.api_permissions();
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
    const URLPatternList host_perms = extension.host_permissions();
    if (!host_perms.empty()) {
      std::vector<URLPattern>::const_iterator host_perms_iter;
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
  bool enabled = service()->extension_prefs()->
      GetExtensionState(extension_id) == Extension::ENABLED;

  DictionaryValue* result = CreateExtensionInfo(*extension, enabled);
  result_.reset(result);

  return true;
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
  Browser::OpenApplication(profile(), extension, launch_container, NULL);
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

  if (!service()->GetExtensionById(extension_id, true)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoExtensionError, extension_id);
    return false;
  }

  ExtensionPrefs* prefs = service()->extension_prefs();
  Extension::State state = prefs->GetExtensionState(extension_id);

  if (state == Extension::DISABLED && enable) {
    service()->EnableExtension(extension_id);
  } else if (state == Extension::ENABLED && !enable) {
    service()->DisableExtension(extension_id);
  }

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

  service()->UninstallExtension(extension_id, false /* external_uninstall */);
  return true;
}

// static
ExtensionManagementEventRouter* ExtensionManagementEventRouter::GetInstance() {
  return Singleton<ExtensionManagementEventRouter>::get();
}

ExtensionManagementEventRouter::ExtensionManagementEventRouter() {}

ExtensionManagementEventRouter::~ExtensionManagementEventRouter() {}

void ExtensionManagementEventRouter::Init() {
  NotificationType::Type types[] = {
    NotificationType::EXTENSION_INSTALLED,
    NotificationType::EXTENSION_UNINSTALLED,
    NotificationType::EXTENSION_LOADED,
    NotificationType::EXTENSION_UNLOADED
  };

  for (size_t i = 0; i < arraysize(types); i++) {
    registrar_.Add(this,
                   types[i],
                   NotificationService::AllSources());
  }
}

void ExtensionManagementEventRouter::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  const char* event_name = NULL;
  switch (type.value) {
    case NotificationType::EXTENSION_INSTALLED:
      event_name = events::kOnExtensionInstalled;
      break;
    case NotificationType::EXTENSION_UNINSTALLED:
      event_name = events::kOnExtensionUninstalled;
      break;
    case NotificationType::EXTENSION_LOADED:
      event_name = events::kOnExtensionEnabled;
      break;
    case NotificationType::EXTENSION_UNLOADED:
      event_name = events::kOnExtensionDisabled;
      break;
    default:
      NOTREACHED();
      return;
  }

  Profile* profile = Source<Profile>(source).ptr();
  CHECK(profile);

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
