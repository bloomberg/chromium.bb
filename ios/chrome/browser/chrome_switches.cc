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

// Disable password generation for iOS.
const char kDisableIOSPasswordGeneration[] = "disable-ios-password-generation";

// Disable showing available password credentials in the keyboard accessory
// view when focused on form fields.
const char kDisableIOSPasswordSuggestions[] =
    "disable-ios-password-suggestions";

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

// Enables Contextual Search.
const char kEnableContextualSearch[] = "enable-contextual-search";

// Enable the experimental Credential Manager JavaScript API.
const char kEnableCredentialManagerAPI[] = "enable-credential-manager-api";

// Enables a workaround for fast inset updates for UIWebView.scrollView.
const char kEnableIOSFastWebScrollViewInsets[] =
    "enable-fast-web-scroll-view-insets";

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

// Enables the recording of metrics reports but disables reporting. In contrast
// to kDisableMetrics, this executes all the code that a normal client would
// use for reporting, except the report is dropped rather than sent to the
// server. This is useful for finding issues in the metrics code during UI and
// performance tests.
const char kIOSMetricsRecordingOnly[] = "metrics-recording-only";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

}  // namespace switches
