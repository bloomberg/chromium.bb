// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if (chrome.test) {
  chrome.test.onMessage.addListener((msg) => {
    if (msg.data != 'preloadZip')
      return;

    console.info('Preloading NaCl module');
    unpacker.app.loadNaclModule(
        unpacker.app.DEFAULT_MODULE_NMF, unpacker.app.DEFAULT_MODULE_TYPE);
  });
  chrome.test.sendMessage(JSON.stringify({name: 'zipArchiverLoaded'}));
} else {
  // Preload the NaCl module the first time the user logs in, or when Chrome OS
  // is updated. This triggers a PNaCl translation, which can be very slow
  // (>10s) on old devices. This prevents a preformance regression when the user
  // tries to zip or unzip files the first time they log in to a device. Using
  // onInstalled should be a compromise between preloading always (inefficient),
  // and doing it lazily (pref regression on first load).
  // See https://crbug.com/907032
  chrome.runtime.onInstalled.addListener((details) => {
    unpacker.app.loadNaclModule(
        unpacker.app.DEFAULT_MODULE_NMF, unpacker.app.DEFAULT_MODULE_TYPE);
  });
}

function setupZipArchiver() {
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

  // Clean all temporary files inside the work directory, just in case the
  // extension aborted previously without removing ones.
  unpacker.app.cleanWorkDirectory();
}

// Load translations
unpacker.app.loadStringData();

// Event called on opening a file with the extension or mime type
// declared in the manifest file.
chrome.app.runtime.onLaunched.addListener(unpacker.app.onLaunched);

// Avoid handling events duplicatedly if this is in incognito context in a
// regular session. https://crbug.com/833603
// onLaunched must be registered without waiting for the profile to resolved,
// or otherwise it misses the first onLaunched event sent right after the
// extension is loaded. https://crbug.com/837251
chrome.fileManagerPrivate.getProfiles((profiles) => {
  if ((profiles[0] && profiles[0].profileId == '$guest') ||
      !chrome.extension.inIncognitoContext) {
    setupZipArchiver();
  } else {
    console.info(
        'The extension was silenced ' +
        'because this is in the incognito context of a regular session.');
  }
});
