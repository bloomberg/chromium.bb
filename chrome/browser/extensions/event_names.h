// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the event names sent to extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_

namespace extensions {

namespace event_names {

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
extern const char kOnFileBrowserGDataPreferencesChanged[];
extern const char kOnFileBrowserNetworkConnectionChanged[];

// InputMethod.
extern const char kOnInputMethodChanged[];

// Context menus.
extern const char kOnContextMenus[];
extern const char kOnContextMenuClicked[];

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
extern const char kBluetoothOnDeviceDiscovered[];
extern const char kBluetoothOnDeviceSearchResult[];
extern const char kBluetoothOnDiscoveringChanged[];
extern const char kBluetoothOnPowerChanged[];
#endif

// Push messaging.
extern const char kOnPushMessage[];

// SystemInfo storage
extern const char kOnStorageAvailableCapacityChanged[];
extern const char kOnStorageAdded[];
extern const char kOnStorageRemoved[];

}  // namespace event_names

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_
