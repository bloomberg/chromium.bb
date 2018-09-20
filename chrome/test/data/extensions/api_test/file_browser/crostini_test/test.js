// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This api testing extension's ID.  Files referenced as Entry will
// have this as part of their URL.
const TEST_EXTENSION_ID = 'pkplfbidichfdicaijlchgnapepdginl';

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
  function testMountCrostini() {
    chrome.fileManagerPrivate.mountCrostini(
        chrome.test.callbackPass());
  },
  function testSharePathWithCrostiniSuccess() {
    getEntry('downloads', 'share_dir').then((entry) => {
      chrome.fileManagerPrivate.sharePathWithCrostini(
          entry, chrome.test.callbackPass());
    });
  },
  function testSharePathWithCrostiniNotDownloads() {
    getEntry('testing', 'test_dir').then((entry) => {
      chrome.fileManagerPrivate.sharePathWithCrostini(
          entry, chrome.test.callbackFail('Path must be under Downloads'));
    });
  },
  function testGetCrostiniSharedPaths() {
    const urlPrefix = 'filesystem:chrome-extension://' + TEST_EXTENSION_ID +
        '/external/Downloads-user';
    chrome.fileManagerPrivate.getCrostiniSharedPaths(
        chrome.test.callbackPass((entries) => {
          chrome.test.assertEq(2, entries.length);
          chrome.test.assertEq(urlPrefix + '/shared1', entries[0].toURL());
          chrome.test.assertTrue(entries[0].isDirectory);
          chrome.test.assertEq('/shared1', entries[0].fullPath);
          chrome.test.assertEq(urlPrefix + '/shared2', entries[1].toURL());
          chrome.test.assertTrue(entries[1].isDirectory);
          chrome.test.assertEq('/shared2', entries[1].fullPath);
        }));
  }
]);
