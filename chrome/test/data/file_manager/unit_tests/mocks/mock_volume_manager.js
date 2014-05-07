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
      VolumeManagerCommon.VolumeType.DRIVE, 'drive'));
  this.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS, 'downloads'));
}

/**
 * Returns the corresponding VolumeInfo.
 *
 * @param {MockFileEntry} entry MockFileEntry pointing anywhere on a volume.
 * @return {VolumeInfo} Corresponding VolumeInfo.
 */
MockVolumeManager.prototype.getVolumeInfo = function(entry) {
  for (var i = 0; i < this.volumeInfoList.length; i++) {
    if (this.volumeInfoList.item(i).volumeId === entry.volumeId)
      return this.volumeInfoList.item(i);
  }
  return null;
};

/**
 * Utility function to create a mock VolumeInfo.
 * @param {VolumeType} type Volume type.
 * @param {string} path Volume path.
 * @return {VolumeInfo} Created mock VolumeInfo.
 */
MockVolumeManager.createMockVolumeInfo = function(type, volumeId) {
  var fileSystem = new MockFileSystem(volumeId);

  var volumeInfo = new VolumeInfo(
      type,
      volumeId,
      fileSystem,
      '',     // error
      '',     // deviceType
      false,  // isReadonly
      {isCurrentProfile: true, displayName: ''},  // profile
      '');    // label

  return volumeInfo;
};
