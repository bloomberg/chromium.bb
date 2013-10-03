// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
#define CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_

#include <string>

#include "chromeos/chromeos_export.h"

namespace chromeos {

// Wrappers of the D-Bus method calls for working with Tpm.
namespace cryptohome_util {

// Returns whether Tpm is presented and enabled.
CHROMEOS_EXPORT bool TpmIsEnabled();

// Returns whether device has already been owned.
CHROMEOS_EXPORT bool TpmIsOwned();

// Returns whether device is being owned (Tpm password is generating).
CHROMEOS_EXPORT bool TpmIsBeingOwned();

CHROMEOS_EXPORT bool InstallAttributesGet(const std::string& name,
                                          std::string* value);
CHROMEOS_EXPORT bool InstallAttributesSet(const std::string& name,
                                          const std::string& value);
CHROMEOS_EXPORT bool InstallAttributesFinalize();
CHROMEOS_EXPORT bool InstallAttributesIsInvalid();
CHROMEOS_EXPORT bool InstallAttributesIsFirstInstall();

}  // namespace cryptohome_util
}  // namespace chromeos

#endif  // CHROMEOS_CRYPTOHOME_CRYPTOHOME_UTIL_H_
