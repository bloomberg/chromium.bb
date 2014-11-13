// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;

/**
 * Set string data.
 * @type {Object}
 */
loadTimeData.data = {
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads',
  DRIVE_DIRECTORY_LABEL: 'Google Drive',
  DRIVE_MY_DRIVE_LABEL: 'My Drive',
  DRIVE_OFFLINE_COLLECTION_LABEL: 'Offline',
  DRIVE_SHARED_WITH_ME_COLLECTION_LABEL: 'Shared with me',
  DRIVE_RECENT_COLLECTION_LABEL: 'Recent'
};

function setUp() {
  chrome = {
    fileManagerPrivate: {
      onDirectoryChanged: {
        addListener: function(listener) { /* Do nothing. */ }
      }
    }
  };

  window.webkitResolveLocalFileSystemURLEntries = {};
  window.webkitResolveLocalFileSystemURL = function(url, callback) {
    callback(webkitResolveLocalFileSystemURLEntries[url]);
  };
}

/**
 * Test case for typical creation of directory tree.
 * This test expect that the following tree is built.
 *
 * Google Drive
 * - My Drive
 * - Shared with me
 * - Recent
 * - Offline
 * Downloads
 *
 * @param {!function(boolean)} callback A callback function which is called with
 *     test result.
 */
function testCreateDirectoryTree(callback) {
  // Create elements.
  var parentElement = document.createElement('div');
  var directoryTree = document.createElement('div');
  parentElement.appendChild(directoryTree);

  // Create mocks.
  var directoryModel = new MockDirectoryModel();
  var volumeManager = new MockVolumeManager();
  var metadataCache = new MockMetadataCache();

  // Set entry which is returned by
  // window.webkitResolveLocalFileSystemURLResults.
  var driveFileSystem = volumeManager.volumeInfoList.item(0).fileSystem;
  window.webkitResolveLocalFileSystemURLEntries['filesystem:drive/root'] =
      new MockDirectoryEntry(driveFileSystem, '/root');

  DirectoryTree.decorate(directoryTree, directoryModel, volumeManager,
      metadataCache, true);
  directoryTree.dataModel = new MockNavigationListModel(volumeManager);
  directoryTree.redraw(true);

  // At top level, Drive and downloads should be listed.
  assertEquals(2, directoryTree.items.length);
  assertEquals(str('DRIVE_DIRECTORY_LABEL'), directoryTree.items[0].label);
  assertEquals(str('DOWNLOADS_DIRECTORY_LABEL'), directoryTree.items[1].label);

  var driveItem = directoryTree.items[0];

  reportPromise(waitUntil(function() {
    // Under the drive item, there exist 4 entries.
    return driveItem.items.length == 4;
  }).then(function() {
    // There exist 1 my drive entry and 3 fake entries under the drive item.
    assertEquals(str('DRIVE_MY_DRIVE_LABEL'), driveItem.items[0].label);
    assertEquals(str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
        driveItem.items[1].label);
    assertEquals(str('DRIVE_RECENT_COLLECTION_LABEL'),
        driveItem.items[2].label);
    assertEquals(str('DRIVE_OFFLINE_COLLECTION_LABEL'),
        driveItem.items[3].label);
  }), callback);
}
