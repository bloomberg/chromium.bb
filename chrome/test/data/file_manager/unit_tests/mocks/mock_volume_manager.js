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
      VolumeManagerCommon.VolumeType.DRIVE, 'drive',
      str('DRIVE_DIRECTORY_LABEL')));
  this.volumeInfoList.push(MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS, 'downloads',
      str('DOWNLOADS_DIRECTORY_LABEL')));
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
 * Obtains location information from an entry.
 * Current implementation can handle only fake entries.
 *
 * @param {Entry} entry A fake entry.
 * @return {EntryLocation} Location information.
 */
MockVolumeManager.prototype.getLocationInfo = function(entry) {
  if (util.isFakeEntry(entry)) {
    return new EntryLocation(this.volumeInfoList.item(0), entry.rootType, true,
        true);
  }

  if (entry.filesystem.name === VolumeManagerCommon.VolumeType.DRIVE) {
    var volumeInfo = this.volumeInfoList.item(0);
    var isRootEntry = entry.fullPath === '/root';
    return new EntryLocation(volumeInfo, VolumeManagerCommon.RootType.DRIVE,
        isRootEntry, true);
  }

  throw new Error('Not implemented exception.');
};

/**
 * Utility function to create a mock VolumeInfo.
 * @param {VolumeType} type Volume type.
 * @param {string} volumeId Volume id.
 * @param {string} label Label.
 * @return {VolumeInfo} Created mock VolumeInfo.
 */
MockVolumeManager.createMockVolumeInfo = function(type, volumeId, label) {
  var fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);
  fileSystem.entries['/'] = new MockDirectoryEntry(fileSystem, '');

  var volumeInfo = new VolumeInfo(
      type,
      volumeId,
      fileSystem,
      '',     // error
      '',     // deviceType
      '',     // devicePath
      false,  // isReadonly
      {isCurrentProfile: true, displayName: ''},  // profile
      label);    // label

  return volumeInfo;
};
