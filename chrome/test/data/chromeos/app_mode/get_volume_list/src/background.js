// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getVolumeList() {
    chrome.fileSystem.getVolumeList(
        chrome.test.callbackPass(function(volumeList) {
          // Drive is not exposed in kiosk session.
          chrome.test.assertEq(1, volumeList.length);
          chrome.test.assertTrue(/^downloads:.*/.test(volumeList[0].volumeId));
          chrome.test.assertTrue(volumeList[0].writable);
        }));
  }
]);
