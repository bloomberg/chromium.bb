// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#include "ios/chrome/browser/experimental_flags.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/chrome_switches.h"

namespace {
NSString* const kEnableAlertOnBackgroundUpload =
    @"EnableAlertsOnBackgroundUpload";
}  // namespace

namespace experimental_flags {

bool IsAlertOnBackgroundUploadEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kEnableAlertOnBackgroundUpload];
}

bool IsOpenFromClipboardEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableIOSOpenFromClipboard);
}

size_t MemoryWedgeSizeInMB() {
  std::string wedge_size_string;

  // Get the size from the Experimental setting.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  wedge_size_string =
      command_line->GetSwitchValueASCII(switches::kIOSMemoryWedgeSize);

  // Otherwise, get from a variation param.
  if (wedge_size_string.empty()) {
    wedge_size_string =
        variations::GetVariationParamValue("MemoryWedge", "wedge_size");
  }

  // Parse the value.
  size_t wedge_size_in_mb = 0;
  if (base::StringToSizeT(wedge_size_string, &wedge_size_in_mb))
    return wedge_size_in_mb;
  return 0;
}

}  // namespace experimental_flags
