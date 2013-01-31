// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/feature.h"

#include <map>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

using chrome::VersionInfo;

namespace {

const VersionInfo::Channel kDefaultChannel = VersionInfo::CHANNEL_STABLE;
VersionInfo::Channel g_current_channel = kDefaultChannel;

}  // namespace

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
chrome::VersionInfo::Channel Feature::GetCurrentChannel() {
  return g_current_channel;
}

// static
void Feature::SetCurrentChannel(VersionInfo::Channel channel) {
  g_current_channel = channel;
}

// static
chrome::VersionInfo::Channel Feature::GetDefaultChannel() {
  return kDefaultChannel;
}

Feature::~Feature() {}

}  // namespace extensions
