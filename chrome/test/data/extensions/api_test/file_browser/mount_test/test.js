// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These have to be sync'd with file_manager_private_apitest.cc
var expectedVolume1 = {
  volumeId: 'removable:mount_path1',
  volumeLabel: 'device_label1',
  sourcePath: 'device_path1',
  volumeType: 'removable',
  deviceType: 'usb',
  devicePath: 'system_path_prefix1',
  isParentDevice: false,
  isReadOnly: false,
  isReadOnlyRemovableDevice: false,
  hasMedia: false,
  configurable: false,
  watchable: true,
  source: 'device',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: 'exfat'
};

var expectedVolume2 = {
  volumeId: 'removable:mount_path2',
  volumeLabel: 'device_label2',
  sourcePath: 'device_path2',
  volumeType: 'removable',
  deviceType: 'mobile',
  devicePath: 'system_path_prefix2',
  isParentDevice: true,
  isReadOnly: true,
  isReadOnlyRemovableDevice: true,
  hasMedia: true,
  configurable: false,
  // This is not an MTP device, so it's watchable.
  // TODO(mtomasz): Add a test for a real MTP device.
  watchable: true,
  source: 'device',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: 'exfat'
};

var expectedVolume3 = {
  volumeId: 'removable:mount_path3',
  volumeLabel: 'device_label3',
  sourcePath: 'device_path3',
  volumeType: 'removable',
  deviceType: 'optical',
  devicePath: 'system_path_prefix3',
  isParentDevice: true,
  isReadOnly: true,
  isReadOnlyRemovableDevice: false,
  hasMedia: false,
  configurable: false,
  watchable: true,
  source: 'device',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: 'exfat'
};

var expectedDownloadsVolume = {
  volumeId: /^downloads:Downloads[^\/]*$/,
  volumeLabel: '',
  volumeType: 'downloads',
  isReadOnly: false,
  isReadOnlyRemovableDevice: false,
  hasMedia: false,
  configurable: false,
  watchable: true,
  source: 'system',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: ''
};

var expectedDriveVolume = {
  volumeId: /^drive:drive[^\/]*$/,
  volumeLabel: '',
  sourcePath: /^\/special\/drive[^\/]*$/,
  volumeType: 'drive',
  isReadOnly: false,
  isReadOnlyRemovableDevice: false,
  hasMedia: false,
  configurable: false,
  watchable: true,
  source: 'network',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: ''
};

var expectedArchiveVolume = {
  volumeId: 'archive:archive_mount_path',
  volumeLabel: 'archive_mount_path',
  sourcePath: /removable\/mount_path3\/archive.zip$/,
  volumeType: 'archive',
  isReadOnly: true,
  isReadOnlyRemovableDevice: false,
  hasMedia: false,
  configurable: false,
  watchable: true,
  source: 'file',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: ''
};

var expectedProvidedVolume = {
  volumeId: 'provided:',
  volumeLabel: '',
  volumeType: 'provided',
  isReadOnly: true,
  isReadOnlyRemovableDevice: false,
  hasMedia: false,
  configurable: true,
  watchable: false,
  extensionId: 'testing-extension-id',
  source: 'network',
  mountContext: 'auto',
  fileSystemId: '',
  profile: {profileId: '', displayName: '', isCurrentProfile: true},
  diskFileSystemType: ''
};

// List of expected mount points.
// NOTE: this has to be synced with values in file_manager_private_apitest.cc
//       and values sorted by volumeId.
var expectedVolumeList = [
  expectedArchiveVolume,
  expectedDownloadsVolume,
  expectedDriveVolume,
  expectedProvidedVolume,
  expectedVolume1,
  expectedVolume2,
  expectedVolume3
];

function validateObject(received, expected, name) {
  for (var key in expected) {
    if (expected[key] instanceof RegExp) {
      if (!expected[key].test(received[key])) {
        console.warn('Expected "' + key + '" ' + name + ' property to match: ' +
                     expected[key] + ', but got: "' + received[key] + '".');
        return false;
      }
    } else if (expected[key] instanceof Object) {
      if (!validateObject(received[key], expected[key], name + "." + key))
        return false;
    } else if (received[key] != expected[key]) {
      console.warn('Expected "' + key + '" ' + name + ' property to be: "' +
                  expected[key] + '"' + ', but got: "' + received[key] +
                  '" instead.');
      return false;
    }
  }

  var expectedKeys = Object.keys(expected);
  var receivedKeys = Object.keys(received);
  if (expectedKeys.length !== receivedKeys.length) {
    var unexpectedKeys = [];
    for (var i = 0; i < receivedKeys.length; i++) {
      if (!(receivedKeys[i] in expected))
        unexpectedKeys.push(receivedKeys[i]);
    }

    console.warn('Unexpected properties found: ' + unexpectedKeys);
    return false;
  }
  return true;
}

chrome.test.runTests([
  function removeMount() {
    chrome.fileManagerPrivate.removeMount('archive:archive_mount_path');

    // We actually check this one on C++ side. If MountLibrary.RemoveMount
    // doesn't get called, test will fail.
    chrome.test.succeed();
  },

  function getVolumeMetadataList() {
    chrome.fileManagerPrivate.getVolumeMetadataList(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(expectedVolumeList.length, result.length,
              'getMountPoints returned wrong number of mount points.');
          for (var i = 0; i < expectedVolumeList.length; i++) {
            chrome.test.assertTrue(
                validateObject(
                    result[i], expectedVolumeList[i], 'volumeMetadata'),
                'getMountPoints result[' + i + '] not as expected');
          }
    }));
  }
]);
