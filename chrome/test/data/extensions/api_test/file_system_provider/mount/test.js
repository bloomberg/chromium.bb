// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function goodDisplayName() {
    chrome.fileSystemProvider.mount(
      'test file system',
      function(fileSystemId) {
        chrome.test.assertEq('number', typeof(fileSystemId));
        chrome.test.assertTrue(fileSystemId == 1);
        chrome.test.succeed();
      },
      function(error) {
        chrome.test.fail();
      }
    );
  },

  function emptyDisplayName() {
    chrome.fileSystemProvider.mount(
      '',
      function(fileSystemId) {
        chrome.test.fail();
      },
      function(error) {
        chrome.test.assertEq('SecurityError', error.name);
        chrome.test.succeed();
      }
    );
  },

  function successfulMount() {
    chrome.fileSystemProvider.mount(
      'caramel-candy.zip',
      function(fileSystemId) {
          chrome.test.assertTrue(fileSystemId > 0);
        chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeList) {
          var found = volumeList.filter(function(volumeInfo) {
            return volumeInfo.volumeId ==
                'provided:' + chrome.runtime.id + '-' + fileSystemId + '-user';
          });
          chrome.test.assertEq(1, found.length);
          chrome.test.succeed();
        });
      },
      function(error) {
        chrome.test.fail();
      });
  },

  function stressMountTest() {
    // Try to create more than allowed number of file systems. All of the mount
    // requests should succeed, except the last one which should fail with a
    // security error.
    var ALREADY_MOUNTED_FILE_SYSTEMS = 2;  // By previous tests.
    var MAX_FILE_SYSTEMS = 16;
    var index = 0;
    var tryNextOne = function() {
      index++;
      if (index < MAX_FILE_SYSTEMS - ALREADY_MOUNTED_FILE_SYSTEMS + 1) {
        chrome.fileSystemProvider.mount(
            index + 'th file system',
            function(fileSystemId) {
              chrome.test.assertTrue(fileSystemId > 0);
              tryNextOne();
            },
            chrome.test.fail);
      } else {
        chrome.fileSystemProvider.mount(
            'over the limit fs',
            chrome.test.fail,
            function(error) {
              chrome.test.assertEq('SecurityError', error.name);
              chrome.test.succeed();
            });
      }
    };
    tryNextOne();
  }
]);
