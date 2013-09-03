// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the event names sent to extensions.

#ifndef CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_
#define CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_

namespace extensions {

namespace event_names {

// FileBrowser.
extern const char kOnDirectoryChanged[];
extern const char kOnFileBrowserMountCompleted[];
extern const char kOnFileTransfersUpdated[];
extern const char kOnDocumentFeedFetched[];
extern const char kOnFileBrowserPreferencesChanged[];
extern const char kOnFileBrowserDriveConnectionStatusChanged[];
extern const char kOnFileBrowserCopyProgress[];

// InputMethod.
extern const char kOnInputMethodChanged[];

// Context menus.
extern const char kOnContextMenus[];

// OffscreenTabs.
extern const char kOnOffscreenTabUpdated[];

// Bluetooth.
extern const char kBluetoothOnDeviceDiscovered[];
extern const char kBluetoothOnDeviceSearchFinished[];
extern const char kBluetoothOnDeviceSearchResult[];

// systemInfo event names.
extern const char kOnDisplayChanged[];
extern const char kOnStorageAvailableCapacityChanged[];

// Notifications.
extern const char kOnNotificationDisplayed[];
extern const char kOnNotificationError[];

}  // namespace event_names

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EVENT_NAMES_H_
