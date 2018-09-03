// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Get specified entry.
 * @param {string} volumeType volume type for entry.
 * @param {string} path path of entry.
 * @return {!Entry} specified entry.
 */
function getEntry(volumeType, path) {
  return new Promise(resolve => {
    chrome.fileManagerPrivate.getVolumeMetadataList(list => {
      const volume = list.find(v => v.volumeType === volumeType);
      chrome.fileSystem.requestFileSystem({volumeId: volume.volumeId}, fs => {
        fs.root.getDirectory(path, {}, entry => {
          resolve(entry);
        });
      });
    });
  });
}

// Run the tests.
chrome.test.runTests([
  function testIsCrostiniEnabled() {
    chrome.fileManagerPrivate.isCrostiniEnabled(
        chrome.test.callbackPass((enabled) => {
          chrome.test.assertTrue(enabled);
        }));
  },
  function testMountCrostiniContainer() {
    chrome.fileManagerPrivate.mountCrostiniContainer(
        chrome.test.callbackPass());
  },
  function testSharePathWithCrostiniContainerSuccess() {
    getEntry('downloads', 'share_dir').then((entry) => {
      chrome.fileManagerPrivate.sharePathWithCrostiniContainer(
          entry, chrome.test.callbackPass());
    });
  },
  function testSharePathWithCrostiniContainerNotDownloads() {
    getEntry('testing', 'test_dir').then((entry) => {
      chrome.fileManagerPrivate.sharePathWithCrostiniContainer(
          entry,
          chrome.test.callbackFail(
              'Share with Linux only allowed for directories within Downloads.'));
    });
  },
]);
