// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_names.h"

namespace extensions {

namespace event_names {

const char kOnDirectoryChanged[] = "fileBrowserPrivate.onDirectoryChanged";
const char kOnFileBrowserMountCompleted[] =
    "fileBrowserPrivate.onMountCompleted";
const char kOnFileTransfersUpdated[] =
    "fileBrowserPrivate.onFileTransfersUpdated";
const char kOnFileBrowserPreferencesChanged[] =
    "fileBrowserPrivate.onPreferencesChanged";
const char kOnFileBrowserDriveConnectionStatusChanged[] =
    "fileBrowserPrivate.onDriveConnectionStatusChanged";
const char kOnFileBrowserCopyProgress[] = "fileBrowserPrivate.onCopyProgress";

const char kOnInputMethodChanged[] = "inputMethodPrivate.onChanged";

const char kOnContextMenus[] = "contextMenus";

const char kOnOffscreenTabUpdated[] = "experimental.offscreenTabs.onUpdated";

const char kBluetoothOnDeviceDiscovered[] = "bluetooth.onDeviceDiscovered";
const char kBluetoothOnDeviceSearchFinished[] =
    "bluetooth.onDeviceSearchFinished";
const char kBluetoothOnDeviceSearchResult[] = "bluetooth.onDeviceSearchResult";

const char kOnDisplayChanged[] = "system.display.onDisplayChanged";
const char kOnStorageAvailableCapacityChanged[] =
    "system.storage.onAvailableCapacityChanged";

const char kOnNotificationDisplayed[] = "notifications.onDisplayed";
const char kOnNotificationError[] = "notifications.onError";

}  // namespace event_names

}  // namespace extensions
