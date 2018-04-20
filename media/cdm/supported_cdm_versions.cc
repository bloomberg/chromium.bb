// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_cdm_versions.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"

namespace media {

namespace {

// Returns the overridden supported CDM interface version speficied on command
// line, which can be null if not specified.
base::Optional<int> GetSupportedCdmInterfaceVersionOverrideFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line)
    return base::nullopt;

  auto version_string = command_line->GetSwitchValueASCII(
      switches::kOverrideEnabledCdmInterfaceVersion);

  int version = 0;
  if (!base::StringToInt(version_string, &version))
    return base::nullopt;
  else
    return version;
}

// Returns whether the CDM interface of |version| is enabled by default.
constexpr bool IsCdmInterfaceVersionEnabledByDefault(int version) {
  switch (version) {
    // Supported versions in decreasing order.
    case 10:
      return CdmInterfaceTraits<10>::IsEnabledByDefault();
    case 9:
      return CdmInterfaceTraits<9>::IsEnabledByDefault();
    default:
      return false;
  }
}

}  // namespace

bool IsSupportedCdmModuleVersion(int version) {
  switch (version) {
    // Latest.
    case CDM_MODULE_VERSION:
      return true;
    default:
      return false;
  }
}

bool IsSupportedAndEnabledCdmInterfaceVersion(int version) {
  if (!IsSupportedCdmInterfaceVersion(version))
    return false;

  auto version_override =
      GetSupportedCdmInterfaceVersionOverrideFromCommandLine();
  if (version_override)
    return version == version_override;

  return IsCdmInterfaceVersionEnabledByDefault(version);
}

}  // namespace media
