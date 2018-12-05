// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_management.h"
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
using permissions_api_helpers::UnpackPermissionSetResult;

namespace {

const char kBlockedByEnterprisePolicy[] =
    "Permissions are blocked by enterprise policy.";
const char kCantRemoveRequiredPermissionsError[] =
    "You cannot remove required permissions.";
const char kNotInManifestPermissionsError[] =
    "Only permissions specified in the manifest may be requested.";
const char kCannotBeOptionalError[] =
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

ExtensionFunction::ResponseAction PermissionsContainsFunction::Run() {
  std::unique_ptr<api::permissions::Contains::Params> params(
      api::permissions::Contains::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // NOTE: The result is not used to make any security decisions. Therefore,
  // it is entirely fine to set |allow_file_access| to true below. This will
  // avoid throwing error when extension() doesn't have access to file://.
  constexpr bool kAllowFileAccess = true;

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      permissions_api_helpers::UnpackPermissionSet(
          params->permissions,
          PermissionsParser::GetRequiredPermissions(extension()),
          PermissionsParser::GetOptionalPermissions(extension()),
          kAllowFileAccess, &error);

  if (!unpack_result)
    return RespondNow(Error(error));

  const PermissionSet& active_permissions =
      extension()->permissions_data()->active_permissions();

  bool has_all_permissions =
      // An extension can never have an active permission that wasn't listed in
      // the manifest, so we know it won't contain all permissions in
      // |unpack_result| if there are any unlisted.
      unpack_result->unlisted_apis.empty() &&
      unpack_result->unlisted_hosts.is_empty() &&
      // Unsupported optional permissions can never be granted, so we know if
      // there are any specified, the extension doesn't actively have them.
      unpack_result->unsupported_optional_apis.empty() &&
      // Otherwise, check each expected location for whether it contains the
      // relevant permissions.
      active_permissions.apis().Contains(unpack_result->optional_apis) &&
      active_permissions.apis().Contains(unpack_result->required_apis) &&
      active_permissions.explicit_hosts().Contains(
          unpack_result->optional_explicit_hosts) &&
      active_permissions.explicit_hosts().Contains(
          unpack_result->required_explicit_hosts) &&
      active_permissions.scriptable_hosts().Contains(
          unpack_result->required_scriptable_hosts);

  return RespondNow(ArgumentList(
      api::permissions::Contains::Results::Create(has_all_permissions)));
}

ExtensionFunction::ResponseAction PermissionsGetAllFunction::Run() {
  std::unique_ptr<Permissions> permissions =
      permissions_api_helpers::PackPermissionSet(
          extension()->permissions_data()->active_permissions());
  return RespondNow(
      ArgumentList(api::permissions::GetAll::Results::Create(*permissions)));
}

ExtensionFunction::ResponseAction PermissionsRemoveFunction::Run() {
  std::unique_ptr<api::permissions::Remove::Params> params(
      api::permissions::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      permissions_api_helpers::UnpackPermissionSet(
          params->permissions,
          PermissionsParser::GetRequiredPermissions(extension()),
          PermissionsParser::GetOptionalPermissions(extension()),
          ExtensionPrefs::Get(browser_context())
              ->AllowFileAccess(extension_->id()),
          &error);

  if (!unpack_result)
    return RespondNow(Error(error));

  // We can't remove any permissions that weren't specified in the manifest.
  if (!unpack_result->unlisted_apis.empty() ||
      !unpack_result->unlisted_hosts.is_empty()) {
    return RespondNow(Error(kNotInManifestPermissionsError));
  }

  if (!unpack_result->unsupported_optional_apis.empty()) {
    return RespondNow(
        Error(kCannotBeOptionalError,
              (*unpack_result->unsupported_optional_apis.begin())->name()));
  }

  // Make sure we only remove optional permissions, and not required
  // permissions. Sadly, for some reason we support having a permission be both
  // optional and required (and should assume its required), so we need both of
  // these checks.
  // TODO(devlin): *Why* do we support that? Should be a load error.
  // NOTE(devlin): This won't support removal of required permissions that can
  // withheld. I don't think that will be a common use case, and so is probably
  // fine.
  if (!unpack_result->required_apis.empty() ||
      !unpack_result->required_explicit_hosts.is_empty() ||
      !unpack_result->required_scriptable_hosts.is_empty()) {
    return RespondNow(Error(kCantRemoveRequiredPermissionsError));
  }

  PermissionSet permissions(
      std::move(unpack_result->optional_apis), ManifestPermissionSet(),
      unpack_result->optional_explicit_hosts, URLPatternSet());

  // Only try and remove those permissions that are active on the extension.
  // For backwards compatability with behavior before this check was added, just
  // silently remove any that aren't present.
  std::unique_ptr<const PermissionSet> permissions_to_revoke =
      PermissionSet::CreateIntersection(
          permissions, extension()->permissions_data()->active_permissions());

  PermissionsUpdater(browser_context())
      .RevokeOptionalPermissions(
          *extension(), *permissions_to_revoke, PermissionsUpdater::REMOVE_SOFT,
          base::BindOnce(
              &PermissionsRemoveFunction::Respond, base::RetainedRef(this),
              ArgumentList(api::permissions::Remove::Results::Create(true))));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

// static
void PermissionsRequestFunction::SetAutoConfirmForTests(bool should_proceed) {
  auto_confirm_for_tests = should_proceed ? PROCEED : ABORT;
}

void PermissionsRequestFunction::ResetAutoConfirmForTests() {
  auto_confirm_for_tests = DO_NOT_SKIP;
}

// static
void PermissionsRequestFunction::SetIgnoreUserGestureForTests(
    bool ignore) {
  ignore_user_gesture_for_tests = ignore;
}

PermissionsRequestFunction::PermissionsRequestFunction() {}

PermissionsRequestFunction::~PermissionsRequestFunction() {}

ExtensionFunction::ResponseAction PermissionsRequestFunction::Run() {
  if (!user_gesture() &&
      !ignore_user_gesture_for_tests &&
      extension_->location() != Manifest::COMPONENT) {
    return RespondNow(Error(kUserGestureRequiredError));
  }

  gfx::NativeWindow native_window =
      ChromeExtensionFunctionDetails(this).GetNativeWindowForUI();
  if (!native_window && auto_confirm_for_tests == DO_NOT_SKIP)
    return RespondNow(Error("Could not find an active window."));

  std::unique_ptr<api::permissions::Request::Params> params(
      api::permissions::Request::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  std::unique_ptr<UnpackPermissionSetResult> unpack_result =
      permissions_api_helpers::UnpackPermissionSet(
          params->permissions,
          PermissionsParser::GetRequiredPermissions(extension()),
          PermissionsParser::GetOptionalPermissions(extension()),
          ExtensionPrefs::Get(browser_context())
              ->AllowFileAccess(extension_->id()),
          &error);

  if (!unpack_result)
    return RespondNow(Error(error));

  // Don't allow the extension to request any permissions that weren't specified
  // in the manifest.
  if (!unpack_result->unlisted_apis.empty() ||
      !unpack_result->unlisted_hosts.is_empty()) {
    return RespondNow(Error(kNotInManifestPermissionsError));
  }

  if (!unpack_result->unsupported_optional_apis.empty()) {
    return RespondNow(
        Error(kCannotBeOptionalError,
              (*unpack_result->unsupported_optional_apis.begin())->name()));
  }

  const PermissionSet& active_permissions =
      extension()->permissions_data()->active_permissions();

  // Determine which of the requested permissions are optional permissions that
  // are "new", i.e. aren't already active on the extension.
  requested_optional_ = std::make_unique<const PermissionSet>(
      std::move(unpack_result->optional_apis), ManifestPermissionSet(),
      unpack_result->optional_explicit_hosts, URLPatternSet());
  requested_optional_ =
      PermissionSet::CreateDifference(*requested_optional_, active_permissions);

  // Do the same for withheld permissions.
  requested_withheld_ = std::make_unique<const PermissionSet>(
      APIPermissionSet(), ManifestPermissionSet(),
      unpack_result->required_explicit_hosts,
      unpack_result->required_scriptable_hosts);
  requested_withheld_ =
      PermissionSet::CreateDifference(*requested_withheld_, active_permissions);

  // Determine the total "new" permissions; this is the set of all permissions
  // that aren't currently active on the extension.
  std::unique_ptr<const PermissionSet> total_new_permissions =
      PermissionSet::CreateUnion(*requested_withheld_, *requested_optional_);

  // If all permissions are already active, nothing left to do.
  if (total_new_permissions->IsEmpty()) {
    constexpr bool granted = true;
    return RespondNow(OneArgument(std::make_unique<base::Value>(granted)));
  }

  // Automatically declines api permissions requests, which are blocked by
  // enterprise policy.
  if (!ExtensionManagementFactory::GetForBrowserContext(browser_context())
           ->IsPermissionSetAllowed(extension(), *total_new_permissions)) {
    return RespondNow(Error(kBlockedByEnterprisePolicy));
  }

  // At this point, all permissions in |requested_withheld_| should be within
  // the |withheld_permissions| section of the PermissionsData.
  DCHECK(extension()->permissions_data()->withheld_permissions().Contains(
      *requested_withheld_));

  // Prompt the user for any new permissions that aren't contained within the
  // already-granted permissions. We don't prompt for already-granted
  // permissions since these were either granted to an earlier extension version
  // or removed by the extension itself (using the permissions.remove() method).
  std::unique_ptr<const PermissionSet> granted_permissions =
      ExtensionPrefs::Get(browser_context())
          ->GetRuntimeGrantedPermissions(extension()->id());
  std::unique_ptr<const PermissionSet> already_granted_permissions =
      PermissionSet::CreateIntersection(*granted_permissions,
                                        *requested_optional_);
  total_new_permissions = PermissionSet::CreateDifference(
      *total_new_permissions, *already_granted_permissions);

  // We don't need to show the prompt if there are no new warnings, or if
  // we're skipping the confirmation UI. COMPONENT extensions are allowed to
  // silently increase their permission level.
  const PermissionMessageProvider* message_provider =
      PermissionMessageProvider::Get();
  // TODO(devlin): We should probably use the same logic we do for permissions
  // increases here, where we check if there are *new* warnings (e.g., so we
  // don't warn about the tabs permission if history is already granted).
  bool has_no_warnings =
      message_provider
          ->GetPermissionMessages(message_provider->GetAllPermissionIDs(
              *total_new_permissions, extension()->GetType()))
          .empty();
  if (has_no_warnings || extension_->location() == Manifest::COMPONENT) {
    OnInstallPromptDone(ExtensionInstallPrompt::Result::ACCEPTED);
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  // Otherwise, we have to prompt the user (though we might "autoconfirm" for a
  // test.
  if (auto_confirm_for_tests != DO_NOT_SKIP) {
    prompted_permissions_for_testing_ = total_new_permissions->Clone();
    if (auto_confirm_for_tests == PROCEED)
      OnInstallPromptDone(ExtensionInstallPrompt::Result::ACCEPTED);
    else if (auto_confirm_for_tests == ABORT)
      OnInstallPromptDone(ExtensionInstallPrompt::Result::USER_CANCELED);
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  install_ui_.reset(new ExtensionInstallPrompt(
      Profile::FromBrowserContext(browser_context()), native_window));
  install_ui_->ShowDialog(
      base::Bind(&PermissionsRequestFunction::OnInstallPromptDone,
                 base::RetainedRef(this)),
      extension(), nullptr,
      std::make_unique<ExtensionInstallPrompt::Prompt>(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT),
      std::move(total_new_permissions),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());

  // ExtensionInstallPrompt::ShowDialog() can call the response synchronously.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void PermissionsRequestFunction::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  if (result != ExtensionInstallPrompt::Result::ACCEPTED) {
    Respond(ArgumentList(api::permissions::Request::Results::Create(false)));
    return;
  }
  PermissionsUpdater permissions_updater(browser_context());
  if (!requested_withheld_->IsEmpty()) {
    requesting_withheld_permissions_ = true;
    permissions_updater.GrantRuntimePermissions(
        *extension(), *requested_withheld_,
        base::BindOnce(&PermissionsRequestFunction::OnRuntimePermissionsGranted,
                       base::RetainedRef(this)));
  }
  if (!requested_optional_->IsEmpty()) {
    requesting_optional_permissions_ = true;
    permissions_updater.GrantOptionalPermissions(
        *extension(), *requested_optional_,
        base::BindOnce(
            &PermissionsRequestFunction::OnOptionalPermissionsGranted,
            base::RetainedRef(this)));
  }
  // Grant{Runtime|Optional}Permissions calls above can finish synchronously.
  if (!did_respond())
    RespondIfRequestsFinished();
}

void PermissionsRequestFunction::OnRuntimePermissionsGranted() {
  requesting_withheld_permissions_ = false;
  RespondIfRequestsFinished();
}

void PermissionsRequestFunction::OnOptionalPermissionsGranted() {
  requesting_optional_permissions_ = false;
  RespondIfRequestsFinished();
}

void PermissionsRequestFunction::RespondIfRequestsFinished() {
  if (requesting_withheld_permissions_ || requesting_optional_permissions_)
    return;

  Respond(ArgumentList(api::permissions::Request::Results::Create(true)));
}

std::unique_ptr<const PermissionSet>
PermissionsRequestFunction::TakePromptedPermissionsForTesting() {
  return std::move(prompted_permissions_for_testing_);
}

}  // namespace extensions
