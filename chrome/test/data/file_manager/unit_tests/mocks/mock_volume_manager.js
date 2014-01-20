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
 * TODO(mtomasz): Remove support for fullpaths.
 *
 * @param {string|MockFileEntry} target Path or MockFileEntry pointing anywhere
 *     on a volume.
 * @return {VolumeInfo} Corresponding VolumeInfo.
 */
MockVolumeManager.prototype.getVolumeInfo = function(target) {
  // TODO(mtomasz): Compare filesystem instances instead.
  var fullPath = target.fullPath || target;
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    if (this.volumeInfoList.item(i).mountPath === fullPath ||
        fullPath.indexOf(this.volumeInfoList.item(i).mountPath) === 0)
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
MockVolumeManager.prototype.resolveAbsolutePath = function(
    path, successCallback, errorCallback) {
  successCallback(new MockFileEntry(path));
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
      '',  // volumeId
      entry,  // root
      '',  // error
      '',  // deviceType
      false,  // isReadonly
      {isCurrentProfile: true, displayName: ''});  // profile

  return volumeInfo;
};
