// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_permissions_api.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

const char kApisKey[] = "permissions";
const char kOriginsKey[] = "origins";

const char kCantRemoveRequiredPermissionsError[] =
    "You cannot remove required permissions.";
const char kNotInOptionalPermissionsError[] =
    "Optional permissions must be listed in extension manifest.";
const char kNotWhitelistedError[] =
    "The optional permissions API does not support '*'.";
const char kUnknownPermissionError[] =
    "'*' is not a recognized permission.";
const char kUserGestureRequiredError[] =
    "This function must be called during a user gesture";
const char kInvalidOrigin[] =
    "Invalid value for origin pattern *: *";

const char kOnAdded[] = "permissions.onAdded";
const char kOnRemoved[] = "permissions.onRemoved";

enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};
AutoConfirmForTest auto_confirm_for_tests = DO_NOT_SKIP;
bool ignore_user_gesture_for_tests = false;

DictionaryValue* PackPermissionsToValue(const ExtensionPermissionSet* set) {
  DictionaryValue* value = new DictionaryValue();

  // Generate the list of API permissions.
  ListValue* apis = new ListValue();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  for (ExtensionAPIPermissionSet::const_iterator i = set->apis().begin();
       i != set->apis().end(); ++i)
    apis->Append(Value::CreateStringValue(info->GetByID(*i)->name()));

  // Generate the list of origin permissions.
  URLPatternSet hosts = set->explicit_hosts();
  ListValue* origins = new ListValue();
  for (URLPatternSet::const_iterator i = hosts.begin(); i != hosts.end(); ++i)
    origins->Append(Value::CreateStringValue(i->GetAsString()));

  value->Set(kApisKey, apis);
  value->Set(kOriginsKey, origins);
  return value;
}

// Creates a new ExtensionPermissionSet from its |value| and passes ownership to
// the caller through |ptr|. Sets |bad_message| to true if the message is badly
// formed. Returns false if the method fails to unpack a permission set.
bool UnpackPermissionsFromValue(DictionaryValue* value,
                                scoped_refptr<ExtensionPermissionSet>* ptr,
                                bool* bad_message,
                                std::string* error) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet apis;
  if (value->HasKey(kApisKey)) {
    ListValue* api_list = NULL;
    if (!value->GetList(kApisKey, &api_list)) {
      *bad_message = true;
      return false;
    }
    for (size_t i = 0; i < api_list->GetSize(); ++i) {
      std::string api_name;
      if (!api_list->GetString(i, &api_name)) {
        *bad_message = true;
        return false;
      }

      ExtensionAPIPermission* permission = info->GetByName(api_name);
      if (!permission) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kUnknownPermissionError, api_name);
        return false;
      }
      apis.insert(permission->id());
    }
  }

  URLPatternSet origins;
  if (value->HasKey(kOriginsKey)) {
    ListValue* origin_list = NULL;
    if (!value->GetList(kOriginsKey, &origin_list)) {
      *bad_message = true;
      return false;
    }
    for (size_t i = 0; i < origin_list->GetSize(); ++i) {
      std::string pattern;
      if (!origin_list->GetString(i, &pattern)) {
        *bad_message = true;
        return false;
      }

      URLPattern origin(Extension::kValidHostPermissionSchemes);
      URLPattern::ParseResult parse_result =
          origin.Parse(pattern, URLPattern::IGNORE_PORTS);
      if (URLPattern::PARSE_SUCCESS != parse_result) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            kInvalidOrigin,
            pattern,
            URLPattern::GetParseResultString(parse_result));
        return false;
      }
      origins.AddPattern(origin);
    }
  }

  *ptr = new ExtensionPermissionSet(apis, origins, URLPatternSet());
  return true;
}

} // namespace

ExtensionPermissionsManager::ExtensionPermissionsManager(
    ExtensionService* extension_service)
    : extension_service_(extension_service) {}

ExtensionPermissionsManager::~ExtensionPermissionsManager() {}

void ExtensionPermissionsManager::AddPermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  scoped_refptr<const ExtensionPermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<ExtensionPermissionSet> total(
      ExtensionPermissionSet::CreateUnion(existing, permissions));
  scoped_refptr<ExtensionPermissionSet> added(
      ExtensionPermissionSet::CreateDifference(total.get(), existing));

  extension_service_->UpdateActivePermissions(extension, total.get());

  // Update the granted permissions so we don't auto-disable the extension.
  extension_service_->GrantPermissions(extension);

  NotifyPermissionsUpdated(ADDED, extension, added.get());
}

void ExtensionPermissionsManager::RemovePermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  scoped_refptr<const ExtensionPermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<ExtensionPermissionSet> total(
      ExtensionPermissionSet::CreateDifference(existing, permissions));
  scoped_refptr<ExtensionPermissionSet> removed(
      ExtensionPermissionSet::CreateDifference(existing, total.get()));

  // We update the active permissions, and not the granted permissions, because
  // the extension, not the user, removed the permissions. This allows the
  // extension to add them again without prompting the user.
  extension_service_->UpdateActivePermissions(extension, total.get());

  NotifyPermissionsUpdated(REMOVED, extension, removed.get());
}

void ExtensionPermissionsManager::DispatchEvent(
    const std::string& extension_id,
    const char* event_name,
    const ExtensionPermissionSet* changed_permissions) {
  Profile* profile = extension_service_->profile();
  if (profile && profile->GetExtensionEventRouter()) {
    ListValue value;
    value.Append(PackPermissionsToValue(changed_permissions));
    std::string json_value;
    base::JSONWriter::Write(&value, false, &json_value);
    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id, event_name, json_value, profile, GURL());
  }
}

void ExtensionPermissionsManager::NotifyPermissionsUpdated(
    EventType event_type,
    const Extension* extension,
    const ExtensionPermissionSet* changed) {
  if (!changed || changed->IsEmpty())
    return;

  UpdatedExtensionPermissionsInfo::Reason reason;
  const char* event_name = NULL;

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    event_name = kOnRemoved;
  } else {
    CHECK_EQ(ADDED, event_type);
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    event_name = kOnAdded;
  }

  // Notify other APIs or interested parties.
  UpdatedExtensionPermissionsInfo info = UpdatedExtensionPermissionsInfo(
      extension, changed, reason);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      content::Source<Profile>(extension_service_->profile()),
      content::Details<UpdatedExtensionPermissionsInfo>(&info));

  // Send the new permissions to the renderers.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* host = i.GetCurrentValue();
    Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
    if (extension_service_->profile()->IsSameProfile(profile))
      host->Send(new ExtensionMsg_UpdatePermissions(
          static_cast<int>(reason),
          extension->id(),
          changed->apis(),
          changed->explicit_hosts(),
          changed->scriptable_hosts()));
  }

  // Trigger the onAdded and onRemoved events in the extension.
  DispatchEvent(extension->id(), event_name, changed);
}

bool ContainsPermissionsFunction::RunImpl() {
  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  std::string error;
  if (!args)
    return false;

  scoped_refptr<ExtensionPermissionSet> permissions;
  if (!UnpackPermissionsFromValue(args, &permissions, &bad_message_, &error_))
    return false;
  CHECK(permissions.get());

  result_.reset(Value::CreateBooleanValue(
      GetExtension()->GetActivePermissions()->Contains(*permissions)));
  return true;
}

bool GetAllPermissionsFunction::RunImpl() {
  result_.reset(PackPermissionsToValue(
      GetExtension()->GetActivePermissions()));
  return true;
}

bool RemovePermissionsFunction::RunImpl() {
  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  if (!args)
    return false;

  scoped_refptr<ExtensionPermissionSet> permissions;
  if (!UnpackPermissionsFromValue(args, &permissions, &bad_message_, &error_))
    return false;
  CHECK(permissions.get());

  const Extension* extension = GetExtension();
  ExtensionPermissionsManager* perms_manager =
      profile()->GetExtensionService()->permissions_manager();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();

  // Make sure they're only trying to remove permissions supported by this API.
  ExtensionAPIPermissionSet apis = permissions->apis();
  for (ExtensionAPIPermissionSet::const_iterator i = apis.begin();
       i != apis.end(); ++i) {
    const ExtensionAPIPermission* api = info->GetByID(*i);
    if (!api->supports_optional()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          kNotWhitelistedError, api->name());
      return false;
    }
  }

  // Make sure we don't remove any required pemissions.
  const ExtensionPermissionSet* required = extension->required_permission_set();
  scoped_refptr<ExtensionPermissionSet> intersection(
      ExtensionPermissionSet::CreateIntersection(permissions.get(), required));
  if (!intersection->IsEmpty()) {
    error_ = kCantRemoveRequiredPermissionsError;
    result_.reset(Value::CreateBooleanValue(false));
    return false;
  }

  perms_manager->RemovePermissions(extension, permissions.get());
  result_.reset(Value::CreateBooleanValue(true));
  return true;
}

// static
void RequestPermissionsFunction::SetAutoConfirmForTests(bool should_proceed) {
  auto_confirm_for_tests = should_proceed ? PROCEED : ABORT;
}

// static
void RequestPermissionsFunction::SetIgnoreUserGestureForTests(
    bool ignore) {
  ignore_user_gesture_for_tests = ignore;
}

RequestPermissionsFunction::RequestPermissionsFunction() {}
RequestPermissionsFunction::~RequestPermissionsFunction() {}

bool RequestPermissionsFunction::RunImpl() {
  if (!user_gesture() && !ignore_user_gesture_for_tests) {
    error_ = kUserGestureRequiredError;
    return false;
  }

  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  if (!args)
    return false;

  if (!UnpackPermissionsFromValue(
          args, &requested_permissions_, &bad_message_, &error_))
    return false;
  CHECK(requested_permissions_.get());

  extension_ = GetExtension();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionPermissionsManager* perms_manager =
      profile()->GetExtensionService()->permissions_manager();
  ExtensionPrefs* prefs = profile()->GetExtensionService()->extension_prefs();

  // Make sure they're only requesting permissions supported by this API.
  ExtensionAPIPermissionSet apis = requested_permissions_->apis();
  for (ExtensionAPIPermissionSet::const_iterator i = apis.begin();
       i != apis.end(); ++i) {
    const ExtensionAPIPermission* api = info->GetByID(*i);
    if (!api->supports_optional()) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          kNotWhitelistedError, api->name());
      return false;
    }
  }

  // The requested permissions must be defined as optional in the manifest.
  if (!extension_->optional_permission_set()->Contains(
          *requested_permissions_)) {
    error_ = kNotInOptionalPermissionsError;
    result_.reset(Value::CreateBooleanValue(false));
    return false;
  }

  // We don't need to prompt the user if the requested permissions are a subset
  // of the granted permissions set.
  const ExtensionPermissionSet* granted =
      prefs->GetGrantedPermissions(extension_->id());
  if (granted && granted->Contains(*requested_permissions_)) {
    perms_manager->AddPermissions(extension_, requested_permissions_.get());
    result_.reset(Value::CreateBooleanValue(true));
    SendResponse(true);
    return true;
  }

  // Filter out the granted permissions so we only prompt for new ones.
  requested_permissions_ = ExtensionPermissionSet::CreateDifference(
      requested_permissions_.get(), granted);

  // Balanced with Release() in InstallUIProceed() and InstallUIAbort().
  AddRef();

  // We don't need to show the prompt if there are no new warnings, or if
  // we're skipping the confirmation UI. All extension types but INTERNAL
  // are allowed to silently increase their permission level.
  if (auto_confirm_for_tests == PROCEED ||
      requested_permissions_->GetWarningMessages().size() == 0) {
    InstallUIProceed();
  } else if (auto_confirm_for_tests == ABORT) {
    // Pretend the user clicked cancel.
    InstallUIAbort(true);
  } else {
    CHECK_EQ(DO_NOT_SKIP, auto_confirm_for_tests);
    install_ui_.reset(new ExtensionInstallUI(profile()));
    install_ui_->ConfirmPermissions(
        this, extension_, requested_permissions_.get());
  }

  return true;
}

void RequestPermissionsFunction::InstallUIProceed() {
  ExtensionPermissionsManager* perms_manager =
      profile()->GetExtensionService()->permissions_manager();

  install_ui_.reset();
  result_.reset(Value::CreateBooleanValue(true));
  perms_manager->AddPermissions(extension_, requested_permissions_.get());

  SendResponse(true);

  Release();
}

void RequestPermissionsFunction::InstallUIAbort(bool user_initiated) {
  install_ui_.reset();
  result_.reset(Value::CreateBooleanValue(false));
  requested_permissions_ = NULL;

  SendResponse(true);
  Release();
}
