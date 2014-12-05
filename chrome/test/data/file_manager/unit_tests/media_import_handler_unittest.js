// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!MockFileOperationManager}
 */
var fileOperationManager;

/**
 * @type {!MockMediaScanner}
 */
var mediaScanner;

/**
 * @type {!importer.MediaImportHandler}
 */
var mediaImporter;

/**
 * @type {!VolumeInfo}
 */
var drive;

function setUp() {
  // Set up string assets.
  loadTimeData.data = {
    DRIVE_DIRECTORY_LABEL: 'My Drive',
    DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
  };

  fileOperationManager = new MockFileOperationManager();

  mediaScanner = new MockMediaScanner();

  var volumeManager = new MockVolumeManager();
  drive = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  // Create fake parented and non-parented roots.
  drive.fileSystem.populate([
    '/root/',
    '/other/'
  ]);

  MockVolumeManager.installMockSingleton(volumeManager);

  mediaImporter =
      new importer.MediaImportHandler(fileOperationManager, mediaScanner);
}

function testImportFrom(callback) {
  // Set up a filesystem with some files.
  var fileSystem = new MockFileSystem('fake-media-volume');
  var filenames = [
    '/DCIM/photos0/IMG00001.jpg',
    '/DCIM/photos0/IMG00002.jpg',
    '/DCIM/photos0/IMG00003.jpg',
    '/DCIM/photos1/IMG00001.jpg',
    '/DCIM/photos1/IMG00002.jpg',
    '/DCIM/photos1/IMG00003.jpg'
  ];
  fileSystem.populate(filenames);
  // Set up a fake destination for the import.
  var destinationFileSystem = new MockFileSystem('fake-destination');
  var destination = function() { return destinationFileSystem.root; };

  // Set up some fake media scan results.
  var media = filenames.map(function(filename) {
    return fileSystem.entries[filename];
  });
  mediaScanner.setScanResults(media);

  // Verify the results when the import operation is kicked off.
  reportPromise(
      fileOperationManager.whenPasteCalled().then(
          function(args) {
            // Verify that the task ID is correct.
            assertEquals(importTask.taskId, args.opt_taskId);
            // Verify that we're copying, not moving, files.
            assertFalse(args.isMove);
            // Verify that the sources are correct.
            assertEntryListEquals(media, args.sourceEntries);
            // Verify that the destination is correct.
            assertEquals(destinationFileSystem.root, args.targetEntry);
          }),
      callback);
  // Kick off an import
  var importTask = mediaImporter.importMedia(fileSystem.root, destination);
}

/**
 * Asserts that two lists contain the same set of Entries.  Entries are deemed
 * to be the same if they point to the same full path.
 */
function assertEntryListEquals(list0, list1) {
  assertEquals(list0.length, list1.length);

  /** @param {!FileEntry} entry */
  var entryToPath = function(entry) { return entry.fullPath; };

  var paths0 = list0.map(entryToPath);
  var paths1 = list1.map(entryToPath);
  paths0.sort();
  paths1.sort();

  paths0.forEach(function(path, index) {
    assertEquals(path, paths1[index]);
  });
}
