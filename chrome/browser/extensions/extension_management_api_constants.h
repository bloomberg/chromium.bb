// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_API_CONSTANTS_H_
#pragma once

namespace extension_management_api_constants {

// Keys used for incoming arguments and outgoing JSON data.
extern const char kAppLaunchUrlKey[];
extern const char kDisabledReasonKey[];
extern const char kHostPermissionsKey[];
extern const char kIconsKey[];
extern const char kIsAppKey[];
extern const char kMayDisableKey[];
extern const char kPermissionsKey[];
extern const char kShowConfirmDialogKey[];
extern const char kSizeKey[];
extern const char kUpdateUrlKey[];
extern const char kUrlKey[];

// Values for outgoing JSON data.
extern const char kDisabledReasonPermissionsIncrease[];
extern const char kDisabledReasonUnknown[];

// Error messages.
extern const char kExtensionCreateError[];
extern const char kGestureNeededForEscalationError[];
extern const char kManifestParseError[];
extern const char kNoExtensionError[];
extern const char kNotAnAppError[];
extern const char kUserCantModifyError[];
extern const char kUninstallCanceledError[];
extern const char kUserDidNotReEnableError[];

}  // namespace extension_management_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_API_CONSTANTS_H_
