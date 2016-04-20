// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"

namespace features {

// All features in alphabetical order.

#if defined(OS_WIN) || defined(OS_MACOSX)
// Enables automatic tab discarding, when the system is in low memory state.
const base::Feature kAutomaticTabDiscarding{"AutomaticTabDiscarding",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// An experimental fullscreen prototype that allows pages to map browser and
// system-reserved keyboard shortcuts.
const base::Feature kExperimentalKeyboardLockUI{
    "ExperimentalKeyboardLockUI", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Enables showing the "This computer will no longer receive Google Chrome
// updates" infobar instead of the "will soon stop receiving" infobar on
// deprecated systems.
const base::Feature kLinuxObsoleteSystemIsEndOfTheLine{
    "LinuxObsoleteSystemIsEndOfTheLine", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enables or disables the Material Design version of chrome://history.
const base::Feature kMaterialDesignHistoryFeature {
  "MaterialDesignHistory", base::FEATURE_DISABLED_BY_DEFAULT
};

#if defined(OS_CHROMEOS)
// Runtime flag that indicates whether this leak detector should be enabled in
// the current instance of Chrome.
const base::Feature kRuntimeMemoryLeakDetector{
    "RuntimeMemoryLeakDetector", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

const base::Feature kSafeSearchUrlReporting{"SafeSearchUrlReporting",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// A new user experience for transitioning into fullscreen and mouse pointer
// lock states. The name is a misnomer (for historical reasons); affects both
// Views and Android builds.
const base::Feature kSimplifiedFullscreenUI{
    "ViewsSimplifiedFullscreenUI",
#if defined(USE_AURA)
    // Windows, Linux, Chrome OS.
    base::FEATURE_ENABLED_BY_DEFAULT,
#else
    // Mac, Android.
    base::FEATURE_DISABLED_BY_DEFAULT,
#endif
};

#if defined(SYZYASAN)
// Enable the deferred free mechanism in the syzyasan module, which helps the
// performance by deferring some work on the critical path to a background
// thread.
const base::Feature kSyzyasanDeferredFree{"SyzyasanDeferredFree",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif

#if defined(OS_CHROMEOS)
// Enables or disables the opt-in IME menu in the language settings page.
const base::Feature kOptInImeMenu{"OptInImeMenu",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

}  // namespace features
