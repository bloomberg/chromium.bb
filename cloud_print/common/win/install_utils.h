// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_COMMON_INSTALL_UTILS_H_
#define CLOUD_PRINT_COMMON_INSTALL_UTILS_H_

#include <wtypes.h>
#include <string>

#include "base/files/file_path.h"
#include "base/string16.h"

namespace cloud_print {

// Sets Google Update registry keys after install or update.
void SetGoogleUpdateKeys(const string16& product_id,
                         const string16& product_name);

// Sets custom error message to show by Google Update installer
void SetGoogleUpdateError(const string16& product_id, const string16& message);

// Deletes Google Update reg keys on product uninstall.
void DeleteGoogleUpdateKeys(const string16& product_id);

// Creates control panel uninstall item.
void CreateUninstallKey(const string16& uninstall_id,
                        const string16& product_name,
                        const std::string& uninstall_switch);

// Deletes control panel uninstall item.
void DeleteUninstallKey(const string16& uninstall_id);

// Returns install location retrieved from control panel uninstall key.
base::FilePath GetInstallLocation(const string16& uninstall_id);

// Returns install location retrieved from control panel uninstall key.
void DeleteProgramDir(const std::string& delete_switch);

// Returns true if path is part of program files.
bool IsProgramsFilesParent(const base::FilePath& path);

}  // namespace cloud_print

#endif  // CLOUD_PRINT_COMMON_INSTALL_UTILS_H_

