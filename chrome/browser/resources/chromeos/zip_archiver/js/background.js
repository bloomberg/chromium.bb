// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Event called on opening a file with the extension or mime type
// declared in the manifest file.
chrome.app.runtime.onLaunched.addListener(unpacker.app.onLaunched);

// Save the state before suspending the event page, so we can resume it
// once new events arrive.
chrome.runtime.onSuspend.addListener(unpacker.app.onSuspend);

chrome.fileSystemProvider.onUnmountRequested.addListener(
    unpacker.app.onUnmountRequested);
chrome.fileSystemProvider.onGetMetadataRequested.addListener(
    unpacker.app.onGetMetadataRequested);
chrome.fileSystemProvider.onReadDirectoryRequested.addListener(
    unpacker.app.onReadDirectoryRequested);
chrome.fileSystemProvider.onOpenFileRequested.addListener(
    unpacker.app.onOpenFileRequested);
chrome.fileSystemProvider.onCloseFileRequested.addListener(
    unpacker.app.onCloseFileRequested);
chrome.fileSystemProvider.onReadFileRequested.addListener(
    unpacker.app.onReadFileRequested);

// Load the PNaCl module.
unpacker.app.loadNaclModule('module.nmf', 'application/x-pnacl');

// Load translations
unpacker.app.loadStringData();
