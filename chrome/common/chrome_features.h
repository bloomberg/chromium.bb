// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/feature_list.h"
#include "extensions/features/features.h"
#include "ppapi/features/features.h"
#include "printing/features/features.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if defined(OS_MACOSX)
extern const base::Feature kAppleScriptExecuteJavaScript;
#endif  // defined(OS_MACOSX)

#if defined(OS_CHROMEOS)
extern const base::Feature kArcMemoryManagement;
#endif  // defined(OS_CHROMEOS)

extern const base::Feature kAssetDownloadSuggestionsFeature;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
extern const base::Feature kAutoDismissingDialogs;
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
extern const base::Feature kAutomaticTabDiscarding;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_LINUX)
extern const base::Feature kBackgroundModeAllowRestart;
#endif  // defined(OS_WIN) || defined(OS_LINUX)

extern const base::Feature kBackspaceGoesBackFeature;

extern const base::Feature kBlockPromptsIfDismissedOften;

extern const base::Feature kBrowserHangFixesExperiment;

#if defined(OS_ANDROID)
extern const base::Feature kConsistentOmniboxGeolocation;
#endif

#if defined(OS_WIN)
extern const base::Feature kDisableFirstRunAutoImportWin;
#endif  // defined(OS_WIN)

extern const base::Feature kDisplayPersistenceToggleInPermissionPrompts;

extern const base::Feature kExpectCTReporting;

extern const base::Feature kExperimentalKeyboardLockUI;

#if defined(OS_CHROMEOS)
extern const base::Feature kHappinessTrackingSystem;
#endif

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const base::Feature kLinuxObsoleteSystemIsEndOfTheLine;
#endif

extern const base::Feature kMaterialDesignBookmarks;

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const base::Feature kMaterialDesignExtensions;
#endif

extern const base::Feature kMaterialDesignHistory;

extern const base::Feature kMaterialDesignSettings;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
extern const base::Feature kMediaRemoting;
extern const base::Feature kMediaRemotingEncrypted;
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

extern const base::Feature kModalPermissionPrompts;

#if defined(OS_MACOSX)
extern const base::Feature kNativeNotifications;
#endif  // defined(OS_MACOSX)

extern const base::Feature kOfflinePageDownloadSuggestionsFeature;

extern const base::Feature kOverrideYouTubeFlashEmbed;

#if BUILDFLAG(ENABLE_PLUGINS)
extern const base::Feature kPreferHtmlOverPlugins;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
extern const base::Feature kPrintScaling;
#endif

extern const base::Feature kPushMessagingBackgroundMode;

#if defined(OS_CHROMEOS)
extern const base::Feature kRuntimeMemoryLeakDetector;
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
extern const base::Feature kRunAllFlashInAllowMode;
#endif

extern const base::Feature kSafeSearchUrlReporting;

extern const base::Feature kSimplifiedFullscreenUI;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
extern const base::Feature kSecurityChip;
#endif

#if defined(SYZYASAN)
extern const base::Feature kSyzyasanDeferredFree;
#endif

extern const base::Feature kUseGroupedPermissionInfobars;

#if defined(OS_CHROMEOS)
extern const base::Feature kOptInImeMenu;

extern const base::Feature kQuickUnlockPin;

extern const base::Feature kEHVInputOnImeMenu;
#endif  // defined(OS_CHROMEOS)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
