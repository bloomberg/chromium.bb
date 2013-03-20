// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_names.h"

namespace extensions {

namespace event_names {

const char kOnTabActivated[] = "tabs.onActivated";
const char kOnTabActiveChanged[] = "tabs.onActiveChanged";
const char kOnTabAttached[] = "tabs.onAttached";
const char kOnTabCreated[] = "tabs.onCreated";
const char kOnTabDetached[] = "tabs.onDetached";
const char kOnTabHighlightChanged[] = "tabs.onHighlightChanged";
const char kOnTabHighlighted[] = "tabs.onHighlighted";
const char kOnTabMoved[] = "tabs.onMoved";
const char kOnTabRemoved[] = "tabs.onRemoved";
const char kOnTabReplaced[] = "tabs.onReplaced";
const char kOnTabSelectionChanged[] = "tabs.onSelectionChanged";
const char kOnTabUpdated[] = "tabs.onUpdated";

const char kOnWindowCreated[] = "windows.onCreated";
const char kOnWindowFocusedChanged[] = "windows.onFocusChanged";
const char kOnWindowRemoved[] = "windows.onRemoved";

const char kOnExtensionInstalled[] = "management.onInstalled";
const char kOnExtensionUninstalled[] = "management.onUninstalled";
const char kOnExtensionEnabled[] = "management.onEnabled";
const char kOnExtensionDisabled[] = "management.onDisabled";

const char kOnDirectoryChanged[] = "fileBrowserPrivate.onDirectoryChanged";
const char kOnFileBrowserMountCompleted[] =
    "fileBrowserPrivate.onMountCompleted";
const char kOnFileTransfersUpdated[] =
    "fileBrowserPrivate.onFileTransfersUpdated";
const char kOnDocumentFeedFetched[] =
    "fileBrowserPrivate.onDocumentFeedFetched";
const char kOnFileBrowserPreferencesChanged[] =
    "fileBrowserPrivate.onPreferencesChanged";
const char kOnFileBrowserDriveConnectionStatusChanged[] =
    "fileBrowserPrivate.onDriveConnectionStatusChanged";

const char kOnInputMethodChanged[] = "inputMethodPrivate.onChanged";

const char kOnContextMenus[] = "contextMenus";
const char kOnContextMenuClicked[] = "contextMenus.onClicked";

const char kOnDialDeviceList[] = "dial.onDeviceList";
const char kOnDialError[] = "dial.onError";

const char kOnDownloadCreated[] = "downloads.onCreated";
const char kOnDownloadChanged[] = "downloads.onChanged";
const char kOnDownloadErased[] = "downloads.onErased";
const char kOnDownloadDeterminingFilename[] = "downloads.onDeterminingFilename";

const char kOnSettingsChanged[] = "storage.onChanged";

const char kOnTerminalProcessOutput[] = "terminalPrivate.onProcessOutput";

const char kOnOffscreenTabUpdated[] = "experimental.offscreenTabs.onUpdated";

const char kOnTabCaptureStatusChanged[] = "tabCapture.onStatusChanged";

const char kBluetoothOnAdapterStateChanged[] =
    "bluetooth.onAdapterStateChanged";
const char kBluetoothOnDeviceDiscovered[] = "bluetooth.onDeviceDiscovered";
const char kBluetoothOnDeviceSearchFinished[] =
    "bluetooth.onDeviceSearchFinished";
const char kBluetoothOnDeviceSearchResult[] = "bluetooth.onDeviceSearchResult";

const char kOnPushMessage[] = "pushMessaging.onMessage";

const char kOnCpuUpdated[] = "experimental.systemInfo.cpu.onUpdated";
const char kOnDisplayChanged[] = "systemInfo.display.onDisplayChanged";
const char kOnStorageAvailableCapacityChanged[] =
    "experimental.systemInfo.storage.onAvailableCapacityChanged";
const char kOnStorageAttached[] = "experimental.systemInfo.storage.onAttached";
const char kOnStorageDetached[] = "experimental.systemInfo.storage.onDetached";

const char kOnSystemIndicatorClicked[] = "systemIndicator.onClicked";

const char kOnServiceStatusChanged[] = "syncFileSystem.onServiceStatusChanged";
const char kOnFileStatusChanged[] = "syncFileSystem.onFileStatusChanged";

const char kOnAttachEventName[] = "mediaGalleriesPrivate.onDeviceAttached";
const char kOnDetachEventName[] = "mediaGalleriesPrivate.onDeviceDetached";
const char kOnGalleryChangedEventName[] =
    "mediaGalleriesPrivate.onGalleryChanged";

const char kOnNotificationDisplayed[] = "notifications.onDisplayed";
const char kOnNotificationError[] = "notifications.onError";
const char kOnNotificationClosed[] = "notifications.onClosed";
const char kOnNotificationClicked[] = "notifications.onClicked";
const char kOnNotificationButtonClicked[] = "notifications.onButtonClicked";

const char kOnNetworksChanged[] = "networkingPrivate.onNetworksChanged";
const char kOnNetworkListChanged[] = "networkingPrivate.onNetworkListChanged";

}  // namespace event_names

}  // namespace extensions
