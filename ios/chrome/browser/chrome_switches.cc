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

// Disable showing available password credentials in the keyboard accessory
// view when focused on form fields.
const char kDisableIOSPasswordSuggestions[] =
    "disable-ios-password-suggestions";

// Disables the 3rd party keyboard omnibox workaround.
const char kDisableThirdPartyKeyboardWorkaround[] =
    "disable-third-party-keyboard-workaround";

// Enables Contextual Search.
const char kEnableContextualSearch[] = "enable-contextual-search";

// Enables support for Handoff from Chrome on iOS to the default browser of
// other Apple devices.
const char kEnableIOSHandoffToOtherDevices[] =
    "enable-ios-handoff-to-other-devices";

// Enables the Spotlight actions.
const char kEnableSpotlightActions[] = "enable-spotlight-actions";

// Enables the 3rd party keyboard omnibox workaround.
const char kEnableThirdPartyKeyboardWorkaround[] =
    "enable-third-party-keyboard-workaround";

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
