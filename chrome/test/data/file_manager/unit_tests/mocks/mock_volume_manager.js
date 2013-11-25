// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for VolumeManager.
 * @constructor
 */
function MockVolumeManager() {
  this.volumeInfoList = new cr.ui.ArrayDataModel([]);

  this.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      util.VolumeType.DRIVE, '/drive'));
  this.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      util.VolumeType.DOWNLOADS, '/Downloads'));
}

/**
 * Returns the corresponding VolumeInfo.
 * @param {string} mountPath Path to be looking for with.
 * @return {VolumeInfo} Corresponding VolumeInfo.
 */
MockVolumeManager.prototype.getVolumeInfo = function(mountPath) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    if (this.volumeInfoList.item(i).mountPath === mountPath)
      return this.volumeInfoList.item(i);
  }
  return null;
};

/**
 * Resolve FileEntry.
 * @param {string} path
 * @param {function(FileEntry)} successCallback Callback on success.
 * @param {function()} errorCallback Callback on error.
 */
MockVolumeManager.prototype.resolvePath = function(
    path, successCallback, errorCallback) {
  var mockFileEntry = new MockFileEntry();
  mockFileEntry.fullPath = path;
  successCallback(mockFileEntry);
};

/**
 * Utility function to create a mock VolumeInfo.
 * @param {VolumeType} type Volume type.
 * @param {string} path Volume path.
 * @return {VolumeInfo} Created mock VolumeInfo.
 */
MockVolumeManager.createMockVolumeInfo = function(type, path) {
  var entry = new MockFileEntry();
  entry.fullPath = path;

  var volumeInfo = new VolumeInfo(
      type,
      path,
      entry,  // Directory entry.
      '',  // error
      '',  // deviceType
      false);  // readonly

  return volumeInfo;
};
