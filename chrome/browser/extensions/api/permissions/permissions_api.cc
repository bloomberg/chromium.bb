// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/permissions.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"

namespace extensions {

using api::permissions::Permissions;

namespace Contains = api::permissions::Contains;
namespace GetAll = api::permissions::GetAll;
namespace Remove = api::permissions::Remove;
namespace Request  = api::permissions::Request;
namespace helpers = permissions_api_helpers;

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

bool PermissionsContainsFunction::RunSync() {
  scoped_ptr<Contains::Params> params(Contains::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<PermissionSet> permissions = helpers::UnpackPermissionSet(
      params->permissions,
      ExtensionPrefs::Get(GetProfile())->AllowFileAccess(extension_->id()),
      &error_);
  if (!permissions.get())
    return false;

  results_ = Contains::Results::Create(
      extension()->permissions_data()->active_permissions()->Contains(
          *permissions.get()));
  return true;
}

bool PermissionsGetAllFunction::RunSync() {
  scoped_ptr<Permissions> permissions = helpers::PackPermissionSet(
      extension()->permissions_data()->active_permissions().get());
  results_ = GetAll::Results::Create(*permissions);
  return true;
}

bool PermissionsRemoveFunction::RunSync() {
  scoped_ptr<Remove::Params> params(Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<PermissionSet> permissions = helpers::UnpackPermissionSet(
      params->permissions,
      ExtensionPrefs::Get(GetProfile())->AllowFileAccess(extension_->id()),
      &error_);
  if (!permissions.get())
    return false;

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
  scoped_refptr<const PermissionSet> required =
      PermissionsParser::GetRequiredPermissions(extension());
  scoped_refptr<PermissionSet> intersection(
      PermissionSet::CreateIntersection(permissions.get(), required));
  if (!intersection->IsEmpty()) {
    error_ = kCantRemoveRequiredPermissionsError;
    return false;
  }

  PermissionsUpdater(GetProfile())
      .RemovePermissions(extension(), permissions.get());
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
  PermissionsUpdater perms_updater(GetProfile());
  perms_updater.AddPermissions(extension(), requested_permissions_.get());

  results_ = Request::Results::Create(true);
  SendResponse(true);

  Release();  // Balanced in RunAsync().
}

void PermissionsRequestFunction::InstallUIAbort(bool user_initiated) {
  SendResponse(true);

  Release();  // Balanced in RunAsync().
}

PermissionsRequestFunction::~PermissionsRequestFunction() {}

bool PermissionsRequestFunction::RunAsync() {
  results_ = Request::Results::Create(false);

  if (!user_gesture() &&
      !ignore_user_gesture_for_tests &&
      extension_->location() != Manifest::COMPONENT) {
    error_ = kUserGestureRequiredError;
    return false;
  }

  scoped_ptr<Request::Params> params(Request::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  requested_permissions_ = helpers::UnpackPermissionSet(
      params->permissions,
      ExtensionPrefs::Get(GetProfile())->AllowFileAccess(extension_->id()),
      &error_);
  if (!requested_permissions_.get())
    return false;

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

  // The requested permissions must be defined as optional in the manifest.
  if (!PermissionsParser::GetOptionalPermissions(extension())
           ->Contains(*requested_permissions_)) {
    error_ = kNotInOptionalPermissionsError;
    return false;
  }

  // We don't need to prompt the user if the requested permissions are a subset
  // of the granted permissions set.
  scoped_refptr<const PermissionSet> granted =
      ExtensionPrefs::Get(GetProfile())
          ->GetGrantedPermissions(extension()->id());
  if (granted.get() && granted->Contains(*requested_permissions_.get())) {
    PermissionsUpdater perms_updater(GetProfile());
    perms_updater.AddPermissions(extension(), requested_permissions_.get());
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
  bool has_no_warnings =
      PermissionMessageProvider::Get()
          ->GetWarningMessages(requested_permissions_, extension()->GetType())
          .empty();
  if (auto_confirm_for_tests == PROCEED || has_no_warnings ||
      extension_->location() == Manifest::COMPONENT) {
    InstallUIProceed();
  } else if (auto_confirm_for_tests == ABORT) {
    // Pretend the user clicked cancel.
    InstallUIAbort(true);
  } else {
    CHECK_EQ(DO_NOT_SKIP, auto_confirm_for_tests);
    install_ui_.reset(new ExtensionInstallPrompt(GetAssociatedWebContents()));
    install_ui_->ConfirmPermissions(
        this, extension(), requested_permissions_.get());
  }

  return true;
}

}  // namespace extensions
