// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the event names sent to extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_NAMES_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_NAMES_H_
#pragma once

namespace extension_event_names {

// Tabs.
extern const char kOnTabActivated[];
extern const char kOnTabActiveChanged[];
extern const char kOnTabAttached[];
extern const char kOnTabCreated[];
extern const char kOnTabDetached[];
extern const char kOnTabHighlightChanged[];
extern const char kOnTabHighlighted[];
extern const char kOnTabMoved[];
extern const char kOnTabRemoved[];
extern const char kOnTabSelectionChanged[];
extern const char kOnTabUpdated[];

// Windows.
extern const char kOnWindowCreated[];
extern const char kOnWindowFocusedChanged[];
extern const char kOnWindowRemoved[];

// Management.
extern const char kOnExtensionInstalled[];
extern const char kOnExtensionUninstalled[];
extern const char kOnExtensionEnabled[];
extern const char kOnExtensionDisabled[];

// FileBrowser.
extern const char kOnFileBrowserDiskChanged[];
extern const char kOnFileChanged[];
extern const char kOnFileBrowserMountCompleted[];
extern const char kOnFileTransfersUpdated[];
extern const char kOnDocumentFeedFetched[];

// InputMethod.
extern const char kOnInputMethodChanged[];

// Downloads.
extern const char kOnDownloadCreated[];
extern const char kOnDownloadChanged[];
extern const char kOnDownloadErased[];

// Settings.
extern const char kOnSettingsChanged[];

// TerminalPrivate.
extern const char kOnTerminalProcessOutput[];

// OffscreenTabs.
extern const char kOnOffscreenTabUpdated[];

#if defined(OS_CHROMEOS)
// Bluetooth.
extern const char kBluetoothOnAvailabilityChanged[];
extern const char kBluetoothOnPowerChanged[];
#endif

};  // namespace extension_event_names

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_NAMES_H_
