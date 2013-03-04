// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These have to be sync'd with file_browser_private_apitest.cc
var expectedVolume1 = {
  devicePath: 'device_path1',
  mountPath: 'removable/mount_path1',
  systemPath: 'system_path1',
  filePath: 'file_path1',
  deviceLabel: 'device_label1',
  driveLabel: 'drive_label1',
  deviceType: 'usb',
  totalSize: 1073741824,
  isParent: false,
  isReadOnly: false,
  hasMedia: false,
  isOnBootDevice: false
};

var expectedVolume2 = {
  devicePath: 'device_path2',
  mountPath: 'removable/mount_path2',
  systemPath: 'system_path2',
  filePath: 'file_path2',
  deviceLabel: 'device_label2',
  driveLabel: 'drive_label2',
  deviceType: 'mobile',
  totalSize: 47723,
  isParent: true,
  isReadOnly: true,
  hasMedia: true,
  isOnBootDevice: true
};

var expectedVolume3 = {
  devicePath: 'device_path3',
  mountPath: 'removable/mount_path3',
  systemPath: 'system_path3',
  filePath: 'file_path3',
  deviceLabel: 'device_label3',
  driveLabel: 'drive_label3',
  deviceType: 'optical',
  totalSize: 0,
  isParent: true,
  isReadOnly: false,
  hasMedia: false,
  isOnBootDevice: true
};

// List of expected mount points.
// NOTE: this has to be synced with values in file_browser_private_apitest.cc
//       and values sorted by mountPath.
var expectedMountPoints = [
  {
    sourcePath: 'archive_path',
    mountPath: 'archive/archive_mount_path',
    mountType: 'file',
    mountCondition: ''
  },
  {
    sourcePath: 'device_path1',
    mountPath: 'removable/mount_path1',
    mountType: 'device',
    mountCondition: ''
  },
  {
    sourcePath: 'device_path2',
    mountPath: 'removable/mount_path2',
    mountType: 'device',
    mountCondition: ''
  },
  {
    sourcePath: 'device_path3',
    mountPath: 'removable/mount_path3',
    mountType: 'device',
    mountCondition: ''
  }
];

function validateObject(received, expected, name) {
  for (var key in expected) {
    if (received[key] != expected[key]) {
      console.warn('Expected "' + key + '" ' + name + ' property to be: "' +
                  expected[key] + '"' + ', but got: "' + received[key] +
                  '" instead.');
      return false;
    }
  }
  if (Object.keys(expected).length != Object.keys(received).length) {
    console.warn('Unexpected property found in returned volume');
    return false;
  }
  return true;
}

function createFileUrl(fileName) {
  var testExtensionId = 'ddammdhioacbehjngdmkjcjbnfginlla';
  var fileUrl = 'filesystem:chrome-extension://' + testExtensionId +
                '/external/' + fileName;
  return fileUrl;
}

chrome.test.runTests([
  function removeMount() {
    // The ID of this extension.
    var fileUrl = createFileUrl('archive/archive_mount_path');

    chrome.fileBrowserPrivate.removeMount(fileUrl);

    // We actually check this one on C++ side. If MountLibrary.RemoveMount
    // doesn't get called, test will fail.
    chrome.test.succeed();
  },

  function getVolumeMetadataValid1() {
    chrome.fileBrowserPrivate.getVolumeMetadata(
        createFileUrl(expectedVolume1.mountPath),
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(
              validateObject(result, expectedVolume1, 'volume'),
              'getVolumeMetadata result for first volume not as expected');
    }));
  },

  function getVolumeMetadataValid2() {
    chrome.fileBrowserPrivate.getVolumeMetadata(
        createFileUrl(expectedVolume2.mountPath),
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(
              validateObject(result, expectedVolume2, 'volume'),
              'getVolumeMetadata result for second volume not as expected');
    }));
  },

  function getVolumeMetadataValid3() {
    chrome.fileBrowserPrivate.getVolumeMetadata(
        createFileUrl(expectedVolume3.mountPath),
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(
              validateObject(result, expectedVolume3, 'volume'),
              'getVolumeMetadata result for third volume not as expected');
    }));
  },

  function getVolumeMetadataNonExistentPath() {
    chrome.fileBrowserPrivate.getVolumeMetadata(
        createFileUrl('removable/non_existent_device_path'),
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(undefined, result);
    }));
  },


  function getVolumeMetadataArchive() {
    chrome.fileBrowserPrivate.getVolumeMetadata(
        createFileUrl('archive/archive_mount_path'),
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(undefined, result);
   }));
  },

  function getVolumeMetadataInvalidPath() {
    chrome.fileBrowserPrivate.getVolumeMetadata(
        'some path',
        chrome.test.callbackFail('Invalid mount path.'));
  },

  function getMountPointsTest() {
    chrome.fileBrowserPrivate.getMountPoints(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(result.length, expectedMountPoints.length,
              'getMountPoints returned wrong number of mount points.');
          for (var i = 0; i < expectedMountPoints.length; i++) {
            chrome.test.assertTrue(
                validateObject(result[i], expectedMountPoints[i], 'mountPoint'),
                'getMountPoints result[' + i + '] not as expected');
          }
    }));
  }
]);
