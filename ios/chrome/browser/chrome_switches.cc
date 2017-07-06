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

// Disable the snapshots lru cache.
const char kDisableLRUSnapshotCache[] = "disable-lru-snapshot-cache";

// Disable auto-reload of error pages if offline.
const char kDisableOfflineAutoReload[] = "disable-offline-auto-reload";

// Disables the tab strip auto scroll new tabs.
const char kDisableTabStripAutoScrollNewTabs[] =
    "disable-tab-strip-autoscroll-new-tabs";

// Disables Physical Web scanning for nearby URLs.
const char kDisableIOSPhysicalWeb[] = "disable-ios-physical-web";

// Disables Request Mobile Site.
const char kDisableRequestMobileSite[] = "disable-request-mobile-site";

// Disables the Suggestions UI
const char kDisableSuggestionsUI[] = "disable-suggestions-ui";

// Disables bookmark reordering.
const char kDisableBookmarkReordering[] = "disable-bookmark-reordering";

// Disables the WKBackForwardList based navigation manager experiment.
const char kDisableSlimNavigationManager[] = "disable-slim-navigation-manager";

// Disables the 3rd party keyboard omnibox workaround.
const char kDisableThirdPartyKeyboardWorkaround[] =
    "disable-third-party-keyboard-workaround";

// Enables Contextual Search.
const char kEnableContextualSearch[] = "enable-contextual-search";

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

// Enables the snapshot lru cache.
const char kEnableLRUSnapshotCache[] = "enable-lru-snapshot-cache";

// Enable auto-reload of error pages if offline.
const char kEnableOfflineAutoReload[] = "enable-offline-auto-reload";

// Enables context-sensitive reader mode button in the toolbar.
const char kEnableReaderModeToolbarIcon[] = "enable-reader-mode-toolbar-icon";

// Enables the Spotlight actions.
const char kEnableSpotlightActions[] = "enable-spotlight-actions";

// Enables Physical Web scanning for nearby URLs.
const char kEnableIOSPhysicalWeb[] = "enable-ios-physical-web";

// Enables the Suggestions UI
const char kEnableSuggestionsUI[] = "enable-suggestions-ui";

// Enables bookmark reordering.
const char kEnableBookmarkReordering[] = "enable-bookmark-reordering";

// Enables the WKBackForwardList based navigation manager experiment.
const char kEnableSlimNavigationManager[] = "enable-slim-navigation-manager";

// Enables the 3rd party keyboard omnibox workaround.
const char kEnableThirdPartyKeyboardWorkaround[] =
    "enable-third-party-keyboard-workaround";

// Forces additional Chrome Variation Ids that will be sent in X-Client-Data
// header, specified as a 64-bit encoded list of numeric experiment ids. Ids
// prefixed with the character "t" will be treated as Trigger Variation Ids.
const char kIOSForceVariationIds[] = "force-variation-ids";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

// These mappings only apply to the host resolver.
const char kIOSHostResolverRules[] = "host-resolver-rules";

// Ignores certificate-related errors.
const char kIOSIgnoreCertificateErrors[] = "ignore-certificate-errors";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kIOSTestingFixedHttpPort[] = "testing-fixed-http-port";
const char kIOSTestingFixedHttpsPort[] = "testing-fixed-https-port";

}  // namespace switches
