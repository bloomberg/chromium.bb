// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/permissions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/gurl.h"

using extensions::api::permissions::Permissions;
using extensions::APIPermission;
using extensions::APIPermissionSet;
using extensions::ErrorUtils;
using extensions::PermissionSet;
using extensions::PermissionsInfo;
using extensions::PermissionsUpdater;

namespace Contains = extensions::api::permissions::Contains;
namespace GetAll = extensions::api::permissions::GetAll;
namespace Remove = extensions::api::permissions::Remove;
namespace Request  = extensions::api::permissions::Request;
namespace helpers = extensions::permissions_api_helpers;

namespace {

const char kCantRemoveRequiredPermissionsError[] =
    "You cannot remove required permissions.";
const char kNotInOptionalPermissionsError[] =
    "Optional permissions must be listed in extension manifest.";
const char kNotWhitelistedError[] =
    "The optional permissions API does not support '*'.";
const char kUserGestureRequiredError[] =
    "This function must be called during a user gesture";

enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};
AutoConfirmForTest auto_confirm_for_tests = DO_NOT_SKIP;
bool ignore_user_gesture_for_tests = false;

}  // namespace

bool PermissionsContainsFunction::RunImpl() {
  scoped_ptr<Contains::Params> params(Contains::Params::Create(*args_));

  scoped_refptr<PermissionSet> permissions =
      helpers::UnpackPermissionSet(params->permissions, &error_);
  if (!permissions.get())
    return false;

  results_ = Contains::Results::Create(
      GetExtension()->GetActivePermissions()->Contains(*permissions));
  return true;
}

bool PermissionsGetAllFunction::RunImpl() {
  scoped_ptr<Permissions> permissions =
      helpers::PackPermissionSet(GetExtension()->GetActivePermissions());
  results_ = GetAll::Results::Create(*permissions);
  return true;
}

bool PermissionsRemoveFunction::RunImpl() {
  scoped_ptr<Remove::Params> params(Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  scoped_refptr<PermissionSet> permissions =
      helpers::UnpackPermissionSet(params->permissions, &error_);
  if (!permissions.get())
    return false;

  const extensions::Extension* extension = GetExtension();

  // Make sure they're only trying to remove permissions supported by this API.
  APIPermissionSet apis = permissions->apis();
  for (APIPermissionSet::const_iterator i = apis.begin();
       i != apis.end(); ++i) {
    if (!i->info()->supports_optional()) {
      error_ = ErrorUtils::FormatErrorMessage(
          kNotWhitelistedError, i->name());
      return false;
    }
  }

  // Make sure we don't remove any required pemissions.
  const PermissionSet* required = extension->required_permission_set();
  scoped_refptr<PermissionSet> intersection(
      PermissionSet::CreateIntersection(permissions.get(), required));
  if (!intersection->IsEmpty()) {
    error_ = kCantRemoveRequiredPermissionsError;
    results_ = Remove::Results::Create(false);
    return false;
  }

  PermissionsUpdater(profile()).RemovePermissions(extension, permissions.get());
  results_ = Remove::Results::Create(true);
  return true;
}

// static
void PermissionsRequestFunction::SetAutoConfirmForTests(bool should_proceed) {
  auto_confirm_for_tests = should_proceed ? PROCEED : ABORT;
}

// static
void PermissionsRequestFunction::SetIgnoreUserGestureForTests(
    bool ignore) {
  ignore_user_gesture_for_tests = ignore;
}

PermissionsRequestFunction::PermissionsRequestFunction() {}

void PermissionsRequestFunction::InstallUIProceed() {
  PermissionsUpdater perms_updater(profile());
  perms_updater.AddPermissions(GetExtension(), requested_permissions_.get());

  results_ = Request::Results::Create(true);
  SendResponse(true);

  Release();  // Balanced in RunImpl().
}

void PermissionsRequestFunction::InstallUIAbort(bool user_initiated) {
  results_ = Request::Results::Create(false);
  SendResponse(true);

  Release();  // Balanced in RunImpl().
}

PermissionsRequestFunction::~PermissionsRequestFunction() {}

bool PermissionsRequestFunction::RunImpl() {
  if (!user_gesture() && !ignore_user_gesture_for_tests) {
    error_ = kUserGestureRequiredError;
    return false;
  }

  scoped_ptr<Request::Params> params(Request::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  requested_permissions_ =
      helpers::UnpackPermissionSet(params->permissions, &error_);
  if (!requested_permissions_.get())
    return false;

  extensions::ExtensionPrefs* prefs =
      profile()->GetExtensionService()->extension_prefs();

  // Make sure they're only requesting permissions supported by this API.
  APIPermissionSet apis = requested_permissions_->apis();
  for (APIPermissionSet::const_iterator i = apis.begin();
       i != apis.end(); ++i) {
    if (!i->info()->supports_optional()) {
      error_ = ErrorUtils::FormatErrorMessage(
          kNotWhitelistedError, i->name());
      return false;
    }
  }

  // Filter out permissions that do not need to be listed in the optional
  // section of the manifest.
  scoped_refptr<extensions::PermissionSet>
      manifest_required_requested_permissions =
          PermissionSet::ExcludeNotInManifestPermissions(
              requested_permissions_.get());

  // The requested permissions must be defined as optional in the manifest.
  if (!GetExtension()->optional_permission_set()->Contains(
          *manifest_required_requested_permissions)) {
    error_ = kNotInOptionalPermissionsError;
    results_ = Request::Results::Create(false);
    return false;
  }

  // We don't need to prompt the user if the requested permissions are a subset
  // of the granted permissions set.
  scoped_refptr<const PermissionSet> granted =
      prefs->GetGrantedPermissions(GetExtension()->id());
  if (granted.get() && granted->Contains(*requested_permissions_)) {
    PermissionsUpdater perms_updater(profile());
    perms_updater.AddPermissions(GetExtension(), requested_permissions_.get());
    results_ = Request::Results::Create(true);
    SendResponse(true);
    return true;
  }

  // Filter out the granted permissions so we only prompt for new ones.
  requested_permissions_ = PermissionSet::CreateDifference(
      requested_permissions_.get(), granted.get());

  AddRef();  // Balanced in InstallUIProceed() / InstallUIAbort().

  // We don't need to show the prompt if there are no new warnings, or if
  // we're skipping the confirmation UI. All extension types but INTERNAL
  // are allowed to silently increase their permission level.
  bool has_no_warnings = requested_permissions_->GetWarningMessages(
      GetExtension()->GetType()).empty();
  if (auto_confirm_for_tests == PROCEED || has_no_warnings) {
    InstallUIProceed();
  } else if (auto_confirm_for_tests == ABORT) {
    // Pretend the user clicked cancel.
    InstallUIAbort(true);
  } else {
    CHECK_EQ(DO_NOT_SKIP, auto_confirm_for_tests);
    install_ui_.reset(new ExtensionInstallPrompt(GetAssociatedWebContents()));
    install_ui_->ConfirmPermissions(
        this, GetExtension(), requested_permissions_.get());
  }

  return true;
}
