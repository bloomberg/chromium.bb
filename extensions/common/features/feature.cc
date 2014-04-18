// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature.h"

#include <map>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "extensions/common/extension.h"

namespace extensions {

// static
Feature::Platform Feature::GetCurrentPlatform() {
#if defined(OS_CHROMEOS)
  return CHROMEOS_PLATFORM;
#elif defined(OS_LINUX)
  return LINUX_PLATFORM;
#elif defined(OS_MACOSX)
  return MACOSX_PLATFORM;
#elif defined(OS_WIN)
  return WIN_PLATFORM;
#else
  return UNSPECIFIED_PLATFORM;
#endif
}

// static
Feature::Availability Feature::CreateAvailability(AvailabilityResult result,
                                                  const std::string& message) {
  return Availability(result, message);
}

Feature::Availability Feature::IsAvailableToExtension(
    const Extension* extension) {
  return IsAvailableToManifest(extension->id(),
                               extension->GetType(),
                               extension->location(),
                               extension->manifest_version());
}

Feature::Feature() : no_parent_(false) {}

Feature::~Feature() {}

}  // namespace extensions
