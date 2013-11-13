// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autotest_private/autotest_private_api.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/autotest_private/autotest_private_api_factory.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#endif

namespace extensions {
namespace {

ListValue* GetHostPermissions(const Extension* ext, bool effective_perm) {
  extensions::URLPatternSet pattern_set;
  if (effective_perm) {
    pattern_set =
        extensions::PermissionsData::GetEffectiveHostPermissions(ext);
  } else {
    pattern_set = ext->GetActivePermissions()->explicit_hosts();
  }

  ListValue* permissions = new ListValue;
  for (extensions::URLPatternSet::const_iterator perm = pattern_set.begin();
       perm != pattern_set.end(); ++perm) {
    permissions->Append(new StringValue(perm->GetAsString()));
  }

  return permissions;
}

ListValue* GetAPIPermissions(const Extension* ext) {
  ListValue* permissions = new ListValue;
  std::set<std::string> perm_list =
      ext->GetActivePermissions()->GetAPIsAsStrings();
  for (std::set<std::string>::const_iterator perm = perm_list.begin();
       perm != perm_list.end(); ++perm) {
    permissions->Append(new StringValue(perm->c_str()));
  }
  return permissions;
}

}  // namespace

bool AutotestPrivateLogoutFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLogoutFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(GetProfile())->test_mode())
    chrome::AttemptUserExit();
  return true;
}

bool AutotestPrivateRestartFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(GetProfile())->test_mode())
    chrome::AttemptRestart();
  return true;
}

bool AutotestPrivateShutdownFunction::RunImpl() {
  scoped_ptr<api::autotest_private::Shutdown::Params> params(
      api::autotest_private::Shutdown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

#if defined(OS_CHROMEOS)
  if (params->force) {
    if (!AutotestPrivateAPIFactory::GetForProfile(GetProfile())->test_mode())
      chrome::ExitCleanly();
    return true;
  }
#endif
  if (!AutotestPrivateAPIFactory::GetForProfile(GetProfile())->test_mode())
    chrome::AttemptExit();
  return true;
}

bool AutotestPrivateLoginStatusFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";

  base::DictionaryValue* result(new base::DictionaryValue);
#if defined(OS_CHROMEOS)
  const chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  const bool is_screen_locked =
      !!chromeos::ScreenLocker::default_screen_locker();

  if (user_manager) {
    result->SetBoolean("isLoggedIn", user_manager->IsUserLoggedIn());
    result->SetBoolean("isOwner", user_manager->IsCurrentUserOwner());
    result->SetBoolean("isScreenLocked", is_screen_locked);
    if (user_manager->IsUserLoggedIn()) {
      result->SetBoolean("isRegularUser",
                         user_manager->IsLoggedInAsRegularUser());
      result->SetBoolean("isGuest", user_manager->IsLoggedInAsGuest());
      result->SetBoolean("isKiosk", user_manager->IsLoggedInAsKioskApp());

      const chromeos::User* user = user_manager->GetLoggedInUser();
      result->SetString("email", user->email());
      result->SetString("displayEmail", user->display_email());

      std::string user_image;
      switch (user->image_index()) {
        case chromeos::User::kExternalImageIndex:
          user_image = "file";
          break;

        case chromeos::User::kProfileImageIndex:
          user_image = "profile";
          break;

        default:
          user_image = base::IntToString(user->image_index());
          break;
      }
      result->SetString("userImage", user_image);
    }
  }
#endif

  SetResult(result);
  return true;
}

bool AutotestPrivateLockScreenFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateLockScreenFunction";
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      RequestLockScreen();
#endif
  return true;
}

bool AutotestPrivateGetExtensionsInfoFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateGetExtensionsInfoFunction";

  ExtensionService* service = extensions::ExtensionSystem::Get(
      GetProfile())->extension_service();
  const ExtensionSet* extensions = service->extensions();
  const ExtensionSet* disabled_extensions = service->disabled_extensions();
  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(GetProfile());

  ListValue* extensions_values = new ListValue;
  ExtensionList all;
  all.insert(all.end(),
             extensions->begin(),
             extensions->end());
  all.insert(all.end(),
             disabled_extensions->begin(),
             disabled_extensions->end());
  for (ExtensionList::const_iterator it = all.begin();
       it != all.end(); ++it) {
    const Extension* extension = it->get();
    std::string id = extension->id();
    DictionaryValue* extension_value = new DictionaryValue;
    extension_value->SetString("id", id);
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("publicKey", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString("backgroundUrl",
        extensions::BackgroundInfo::GetBackgroundURL(extension).spec());
    extension_value->SetString("optionsUrl",
        extensions::ManifestURL::GetOptionsPage(extension).spec());

    extension_value->Set("hostPermissions",
                         GetHostPermissions(extension, false));
    extension_value->Set("effectiveHostPermissions",
                         GetHostPermissions(extension, true));
    extension_value->Set("apiPermissions", GetAPIPermissions(extension));

    Manifest::Location location = extension->location();
    extension_value->SetBoolean("isComponent",
                                location == Manifest::COMPONENT);
    extension_value->SetBoolean("isInternal",
                                location == Manifest::INTERNAL);
    extension_value->SetBoolean("isUserInstalled",
        location == Manifest::INTERNAL ||
        Manifest::IsUnpackedLocation(location));
    extension_value->SetBoolean("isEnabled", service->IsExtensionEnabled(id));
    extension_value->SetBoolean("allowedInIncognito",
        extension_util::IsIncognitoEnabled(id, service));
    extension_value->SetBoolean(
        "hasPageAction",
        extension_action_manager->GetPageAction(*extension) != NULL);

    extensions_values->Append(extension_value);
  }

  DictionaryValue* return_value(new DictionaryValue);
  return_value->Set("extensions", extensions_values);
  SetResult(return_value);
  return true;
}

static int AccessArray(const volatile int arr[], const volatile int *index) {
  return arr[*index];
}

bool AutotestPrivateSimulateAsanMemoryBugFunction::RunImpl() {
  DVLOG(1) << "AutotestPrivateSimulateAsanMemoryBugFunction";
  if (!AutotestPrivateAPIFactory::GetForProfile(GetProfile())->test_mode()) {
    // This array is volatile not to let compiler optimize us out.
    volatile int testarray[3] = {0, 0, 0};

    // Cause Address Sanitizer to abort this process.
    volatile int index = 5;
    AccessArray(testarray, &index);
  }
  return true;
}


AutotestPrivateAPI::AutotestPrivateAPI() : test_mode_(false) {
}

AutotestPrivateAPI::~AutotestPrivateAPI() {
}

}  // namespace extensions
