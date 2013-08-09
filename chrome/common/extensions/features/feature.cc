// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/feature.h"

#include <map>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace extensions {

// static
Feature::Platform Feature::GetCurrentPlatform() {
#if defined(OS_CHROMEOS)
  return CHROMEOS_PLATFORM;
#else
  return UNSPECIFIED_PLATFORM;
#endif
}

// static
Feature::Location Feature::ConvertLocation(Manifest::Location location) {
  if (location == Manifest::COMPONENT)
    return COMPONENT_LOCATION;
  else
    return UNSPECIFIED_LOCATION;
}

// static
Feature::Availability Feature::CreateAvailability(AvailabilityResult result,
                                                  const std::string& message) {
  return Availability(result, message);
}

Feature::Feature() : no_parent_(false) {}

Feature::~Feature() {}

}  // namespace extensions
