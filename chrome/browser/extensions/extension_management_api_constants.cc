// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_api_constants.h"

namespace extension_management_api_constants {

const char kAppLaunchUrlKey[] = "appLaunchUrl";
const char kDisabledReasonKey[] = "disabledReason";
const char kHostPermissionsKey[] = "hostPermissions";
const char kIconsKey[] = "icons";
const char kIsAppKey[] = "isApp";
const char kMayDisableKey[] = "mayDisable";
const char kPermissionsKey[] = "permissions";
const char kShowConfirmDialogKey[] = "showConfirmDialog";
const char kSizeKey[] = "size";
const char kUpdateUrlKey[] = "updateUrl";
const char kUrlKey[] = "url";

const char kDisabledReasonPermissionsIncrease[] = "permissions_increase";
const char kDisabledReasonUnknown[] = "unknown";

const char kExtensionCreateError[] =
    "Failed to create extension from manifest.";
const char kGestureNeededForEscalationError[] =
    "Re-enabling an extension disabled due to permissions increase "
    "requires a user gesture";
const char kManifestParseError[] = "Failed to parse manifest.";
const char kNoExtensionError[] = "Failed to find extension with id *";
const char kNotAnAppError[] = "Extension * is not an App";
const char kUserCantModifyError[] = "Extension * cannot be modified by user";
const char kUninstallCanceledError[] = "Extension * uninstall canceled by user";
const char kUserDidNotReEnableError[] =
    "The user did not accept the re-enable dialog";

}  // namespace extension_management_api_constants
