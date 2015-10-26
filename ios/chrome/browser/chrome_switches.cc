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

// Disables the blocking of external URL requests from sub-frames not initiated
// by user interaction.
const char kDisableIOSBlockUnpromptedExternalURLs[] =
    "disable-block-external-urls";

// Disables the use of WKWebView instead of UIWebView.
const char kDisableIOSWKWebView[] = "disable-wkwebview";

// Enables the blocking of external URL requests from sub-frames not initiated
// by user interaction.
const char kEnableIOSBlockUnpromptedExternalURLs[] =
    "enable-block-external-urls";

// Enables the use of WKWebView instead of UIWebView.
const char kEnableIOSWKWebView[] = "enable-wkwebview";

// Enables context-sensitive reader mode button in the toolbar.
const char kEnableReaderModeToolbarIcon[] = "enable-reader-mode-toolbar-icon";

// Disables support for keyboard commands.
const char kDisableKeyboardCommands[] = "disable-keyboard-commands";

// Enables the recording of metrics reports but disables reporting. In contrast
// to kDisableMetrics, this executes all the code that a normal client would
// use for reporting, except the report is dropped rather than sent to the
// server. This is useful for finding issues in the metrics code during UI and
// performance tests.
const char kIOSMetricsRecordingOnly[] = "metrics-recording-only";

}  // namespace switches
