// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These have to be sync'd with file_browser_private_apitest.cc
var expectedVolume1 = {
  volumeId: 'removable:mount_path1',
  mountPath: '/removable/mount_path1',
  sourcePath: 'device_path1',
  volumeType: 'removable',
  deviceType: 'usb',
  isReadOnly: false
};

var expectedVolume2 = {
  volumeId: 'removable:mount_path2',
  mountPath: '/removable/mount_path2',
  sourcePath: 'device_path2',
  volumeType: 'removable',
  deviceType: 'mobile',
  isReadOnly: true
};

var expectedVolume3 = {
  volumeId: 'removable:mount_path3',
  mountPath: '/removable/mount_path3',
  sourcePath: 'device_path3',
  volumeType: 'removable',
  deviceType: 'optical',
  isReadOnly: false
};

var expectedDownloadsVolume = {
  volumeId: 'downloads:Downloads',
  mountPath: '/Downloads',
  volumeType: 'downloads',
  isReadOnly: false
};

var expectedDriveVolume = {
  volumeId: 'drive:drive',
  mountPath: '/drive',
  sourcePath: '/special/drive',
  volumeType: 'drive',
  isReadOnly: false
};

var expectedArchiveVolume = {
  volumeId: 'archive:archive_mount_path',
  mountPath: '/archive/archive_mount_path',
  sourcePath: 'archive_path',
  volumeType: 'archive',
  isReadOnly: false
};

// List of expected mount points.
// NOTE: this has to be synced with values in file_browser_private_apitest.cc
//       and values sorted by mountPath.
var expectedVolumeList = [
  expectedDriveVolume,
  expectedDownloadsVolume,
  expectedArchiveVolume,
  expectedVolume1,
  expectedVolume2,
  expectedVolume3,
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

  function getVolumeMetadataList() {
    chrome.fileBrowserPrivate.getVolumeMetadataList(
        chrome.test.callbackPass(function(result) {
          chrome.test.assertEq(result.length, expectedVolumeList.length,
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
