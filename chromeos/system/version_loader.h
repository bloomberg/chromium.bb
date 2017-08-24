// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_VERSION_LOADER_H_
#define CHROMEOS_SYSTEM_VERSION_LOADER_H_

#include <string>

#include "base/callback_forward.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace version_loader {

enum VersionFormat {
  VERSION_SHORT,
  VERSION_SHORT_WITH_DATE,
  VERSION_FULL,
};

using StringCallback = base::Callback<void(const std::string&)>;

// Gets the version.
// If |full_version| is true version string with extra info is extracted,
// otherwise it's in short format x.x.xx.x.
// May block.
CHROMEOS_EXPORT std::string GetVersion(VersionFormat format);

// Gets the full OS version and adds the TPM version information. Asynchronous,
// result is passed on to callback as a string.
CHROMEOS_EXPORT void GetFullOSAndTpmVersion(StringCallback callback);

// Gets the ARC version.
// May block.
CHROMEOS_EXPORT std::string GetARCVersion();

// Gets the firmware info.
// May block.
CHROMEOS_EXPORT std::string GetFirmware();

// Extracts the firmware from the file.
CHROMEOS_EXPORT std::string ParseFirmware(const std::string& contents);

}  // namespace version_loader
}  // namespace chromeos

#endif  // CHROMEOS_SYSTEM_VERSION_LOADER_H_
