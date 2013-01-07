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
extern const char kOnDirectoryChanged[];
extern const char kOnFileBrowserMountCompleted[];
extern const char kOnFileTransfersUpdated[];
extern const char kOnDocumentFeedFetched[];
extern const char kOnFileBrowserPreferencesChanged[];
extern const char kOnFileBrowserNetworkConnectionChanged[];

// InputMethod.
extern const char kOnInputMethodChanged[];

// Context menus.
extern const char kOnContextMenus[];
extern const char kOnContextMenuClicked[];

// DIAL.
extern const char kOnDialDeviceList[];

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

// Tab content capture.
extern const char kOnTabCaptureStatusChanged[];

// Bluetooth.
extern const char kBluetoothOnAdapterStateChanged[];
extern const char kBluetoothOnDeviceDiscovered[];
extern const char kBluetoothOnDeviceSearchFinished[];
extern const char kBluetoothOnDeviceSearchResult[];

// Push messaging.
extern const char kOnPushMessage[];

// systemInfo event names.
extern const char kOnCpuUpdated[];
extern const char kOnStorageAvailableCapacityChanged[];
extern const char kOnStorageAdded[];
extern const char kOnStorageRemoved[];

// System Indicator icon.
extern const char kOnSystemIndicatorClicked[];

// SyncFileSystem.
extern const char kOnSyncStateChanged[];
extern const char kOnFileSynced[];

// MediaGalleriesPrivate.
extern const char kOnAttachEventName[];
extern const char kOnDetachEventName[];
extern const char kOnGalleryChangedEventName[];

// Notifications.
extern const char kOnNotificationDisplayed[];
extern const char kOnNotificationError[];
extern const char kOnNotificationClosed[];
extern const char kOnNotificationClicked[];
extern const char kOnNotificationButtonClicked[];

}  // namespace event_names

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_
