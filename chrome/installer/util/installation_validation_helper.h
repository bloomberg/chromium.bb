// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares helper functions for use in tests that expect a valid
// installation, possibly of a specific type.  Validation violations result in
// test failures.

#ifndef CHROME_INSTALLER_UTIL_INSTALLATION_VALIDATION_HELPER_H_
#define CHROME_INSTALLER_UTIL_INSTALLATION_VALIDATION_HELPER_H_
#pragma once

#include "chrome/installer/util/installation_validator.h"

namespace installer {

class InstallationState;

// Evaluates the machine's current installation at level |system_level|.
// Returns the type of installation found.
InstallationValidator::InstallationType ExpectValidInstallation(
    bool system_level);

// Evaluates |machine_state| at level |system_level|.  Returns the type of
// installation found.
InstallationValidator::InstallationType ExpectValidInstallationForState(
    const InstallationState& machine_state,
    bool system_level);

// Evaluates the machine's current installation at level |system_level|,
// expecting an installation of the given |type|.
void ExpectInstallationOfType(
    bool system_level,
    InstallationValidator::InstallationType type);

// Evaluates |machine_state| at level |system_level|, expecting an installation
// of the given |type|.
void ExpectInstallationOfTypeForState(
    const InstallationState& machine_state,
    bool system_level,
    InstallationValidator::InstallationType type);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLATION_VALIDATION_HELPER_H_
