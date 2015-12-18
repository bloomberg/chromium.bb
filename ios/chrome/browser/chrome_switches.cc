// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/chrome_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Disables Contextual Search.
const char kDisableContextualSearch[] = "disable-contextual-search";

// Disables a workaround for fast inset updates for UIWebView.scrollView.
const char kDisableIOSFastWebScrollViewInsets[] =
    "disable-fast-web-scroll-view-insets";

// Lists separated by commas the name of features to disable.
// See base::FeatureList::InitializeFromCommandLine for details.
const char kDisableIOSFeatures[] = "disable-features";

// Disable password generation for iOS.
const char kDisableIOSPasswordGeneration[] = "disable-ios-password-generation";

// Disable showing available password credentials in the keyboard accessory
// view when focused on form fields.
const char kDisableIOSPasswordSuggestions[] =
    "disable-ios-password-suggestions";

// Disables the backend service for web resources.
const char kDisableIOSWebResources[] = "disable-web-resources";

// Disables the use of WKWebView instead of UIWebView.
const char kDisableIOSWKWebView[] = "disable-wkwebview";

// Disables support for keyboard commands.
const char kDisableKeyboardCommands[] = "disable-keyboard-commands";

// Disables NTP favicons.
const char kDisableNTPFavicons[] = "disable-ntp-favicons";

// Disable auto-reload of error pages if offline.
const char kDisableOfflineAutoReload[] = "disable-offline-auto-reload";

// Disables the tab switcher.
const char kDisableTabSwitcher[] = "disable-tab-switcher";

// Disable the snapshots lru cache.
const char kDisableLRUSnapshotCache[] = "disable-lru-snapshot-cache";

// Disables the tab eviction policy. (applicable iff WKWebView is enabled)
const char kDisableTabEviction[] = "disable-tab-eviction";

// Enables Contextual Search.
const char kEnableContextualSearch[] = "enable-contextual-search";

// Enable the experimental Credential Manager JavaScript API.
const char kEnableCredentialManagerAPI[] = "enable-credential-manager-api";

// Enables a workaround for fast inset updates for UIWebView.scrollView.
const char kEnableIOSFastWebScrollViewInsets[] =
    "enable-fast-web-scroll-view-insets";

// Lists separated by commas the name of features to disable.
// See base::FeatureList::InitializeFromCommandLine for details.
const char kEnableIOSFeatures[] = "enable-features";

// Enables support for Handoff from Chrome on iOS to the default browser of
// other Apple devices.
const char kEnableIOSHandoffToOtherDevices[] =
    "enable-ios-handoff-to-other-devices";

// Enable password generation for iOS.
const char kEnableIOSPasswordGeneration[] = "enable-ios-password-generation";

// Enables the use of WKWebView instead of UIWebView.
const char kEnableIOSWKWebView[] = "enable-wkwebview";

// Enables NTP favicons.
const char kEnableNTPFavicons[] = "enable-ntp-favicons";

// Enable auto-reload of error pages if offline.
const char kEnableOfflineAutoReload[] = "enable-offline-auto-reload";

// Enables context-sensitive reader mode button in the toolbar.
const char kEnableReaderModeToolbarIcon[] = "enable-reader-mode-toolbar-icon";

// Enables the tab switcher.
const char kEnableTabSwitcher[] = "enable-tab-switcher";

// Enables the snapshot lru cache.
const char kEnableLRUSnapshotCache[] = "enable-lru-snapshot-cache";

// Enables the tab eviction policy. (applicable iff WKWebView is enabled)
const char kEnableTabEviction[] = "enable-tab-eviction";

// Forces additional Chrome Variation Ids that will be sent in X-Client-Data
// header, specified as a 64-bit encoded list of numeric experiment ids. Ids
// prefixed with the character "t" will be treated as Trigger Variation Ids.
const char kIOSForceVariationIds[] = "force-variation-ids";

// Enables the recording of metrics reports but disables reporting. In contrast
// to kDisableMetrics, this executes all the code that a normal client would
// use for reporting, except the report is dropped rather than sent to the
// server. This is useful for finding issues in the metrics code during UI and
// performance tests.
const char kIOSMetricsRecordingOnly[] = "metrics-recording-only";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

// These mappings only apply to the host resolver.
const char kIOSHostResolverRules[] = "host-resolver-rules";

// Ignores certificate-related errors.
const char kIOSIgnoreCertificateErrors[] = "ignore-certificate-errors";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kIOSTestingFixedHttpPort[] = "testing-fixed-http-port";
const char kIOSTestingFixedHttpsPort[] = "testing-fixed-https-port";

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise in the
// measurements.
const char kDisableBackgroundNetworking[] = "disable-background-networking";

// Disables the dinosaur easter egg on the offline interstitial.
const char kDisableDinosaurEasterEgg[] = "disable-dinosaur-easter-egg";

// Enables grouping websites by domain and filtering them by period.
const char kHistoryEnableGroupByDomain[] = "enable-grouped-history";

// Use to opt-in to marking HTTP as non-secure.
const char kMarkNonSecureAs[] = "mark-non-secure-as";
const char kMarkNonSecureAsNeutral[] = "neutral";
const char kMarkNonSecureAsNonSecure[] = "non-secure";

// If present, safebrowsing only performs update when
// SafeBrowsingProtocolManager::ForceScheduleNextUpdate() is explicitly called.
// This is used for testing only.
const char kSbDisableAutoUpdate[] = "safebrowsing-disable-auto-update";

// Command line flag offering a "Show saved copy" option to the user if offline.
// The various modes are disabled, primary, or secondary. Primary/secondary
// refers to button placement (for experiment).
const char kShowSavedCopy[] = "show-saved-copy";

// Values for the kShowSavedCopy flag.
const char kEnableShowSavedCopyPrimary[] = "primary";
const char kEnableShowSavedCopySecondary[] = "secondary";
const char kDisableShowSavedCopy[] = "disable";

}  // namespace switches
