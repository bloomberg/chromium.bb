// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimalist dummy implementation of about_flags.h for Android.

#include "chrome/browser/about_flags.h"

namespace about_flags {

void ConvertFlagsToSwitches(PrefService*, CommandLine*) {}

void SetExperimentEnabled(PrefService*, const std::string&, bool) {}

void RemoveFlagsSwitches(std::map<std::string, CommandLine::StringType>*) {}

void RecordUMAStatistics(const PrefService*) {}

}  // namespace about_flags
