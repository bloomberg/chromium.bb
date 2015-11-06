// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
#define IOS_CHROME_BROWSER_CHROME_SWITCHES_H_

// Defines all the command-line switches used by iOS Chrome.

namespace switches {

extern const char kDisableContextualSearch[];
extern const char kDisableIOSFastWebScrollViewInsets[];
extern const char kDisableIOSPasswordGeneration[];
extern const char kDisableIOSPasswordSuggestions[];
extern const char kDisableIOSWKWebView[];
extern const char kDisableKeyboardCommands[];
extern const char kDisableNTPFavicons[];
extern const char kDisableOfflineAutoReload[];
extern const char kDisableTabSwitcher[];
extern const char kDisableLRUSnapshotCache[];

extern const char kEnableContextualSearch[];
extern const char kEnableCredentialManagerAPI[];
extern const char kEnableIOSFastWebScrollViewInsets[];
extern const char kEnableIOSHandoffToOtherDevices[];
extern const char kEnableIOSPasswordGeneration[];
extern const char kEnableIOSWKWebView[];
extern const char kEnableNTPFavicons[];
extern const char kEnableOfflineAutoReload[];
extern const char kEnableReaderModeToolbarIcon[];
extern const char kEnableTabSwitcher[];
extern const char kEnableLRUSnapshotCache[];

extern const char kIOSMetricsRecordingOnly[];
extern const char kUserAgent[];

}  // namespace switches

#endif  // IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
