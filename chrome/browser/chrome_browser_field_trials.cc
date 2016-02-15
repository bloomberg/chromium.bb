// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_persistence.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#else
#include "chrome/browser/chrome_browser_field_trials_desktop.h"
#endif

namespace {

// Check for feature enabling the use of persistent histogram storage and
// create an appropriate allocator for such if so.
void InstantiatePersistentHistograms() {
  if (base::FeatureList::IsEnabled(base::kPersistentHistogramsFeature)) {
    const char kAllocatorName[] = "BrowserMetrics";
    // Create persistent/shared memory and allow histograms to be stored in it.
    // TODO(bcwhite): Update this with correct allocator and memory size.
    base::SetPersistentHistogramMemoryAllocator(
        new base::LocalPersistentMemoryAllocator(2 << 20,     // 2 MiB
                                                 0x935DDD43,  // SHA1(B...M...)
                                                 kAllocatorName));
    base::GetPersistentHistogramMemoryAllocator()->CreateTrackingHistograms(
        kAllocatorName);
  }
}

}  // namespace

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(
    const base::CommandLine& parsed_command_line)
    : parsed_command_line_(parsed_command_line) {
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials() {
  // Field trials that are shared by all platforms.
  InstantiateDynamicTrials();

#if defined(OS_ANDROID) || defined(OS_IOS)
  chrome::SetupMobileFieldTrials(parsed_command_line_);
#else
  chrome::SetupDesktopFieldTrials(parsed_command_line_);
#endif
}

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // Persistent histograms must be enabled as soon as possible.
  InstantiatePersistentHistograms();
}
