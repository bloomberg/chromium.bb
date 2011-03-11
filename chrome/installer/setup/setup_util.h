// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#pragma once

#include "base/version.h"

class FilePath;

namespace installer {

class InstallerState;

// Apply a diff patch to source file. First tries to apply it using courgette
// since it checks for courgette header and fails quickly. If that fails
// tries to apply the patch using regular bsdiff. Returns status code.
// The installer stage is updated if |installer_state| is non-NULL.
int ApplyDiffPatch(const FilePath& src,
                   const FilePath& patch,
                   const FilePath& dest,
                   const InstallerState* installer_state);

// Find the version of Chrome from an install source directory.
// Chrome_path should contain at least one version folder.
// Returns the maximum version found or NULL if no version is found.
Version* GetMaxVersionFromArchiveDir(const FilePath& chrome_path);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
