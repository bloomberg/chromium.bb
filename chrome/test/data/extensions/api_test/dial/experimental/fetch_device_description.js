// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function fetchDeviceDecription() {
      const fetchDeviceDescriptionCallback = result => {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('object', typeof(result));
        chrome.test.assertEq('http://127.0.0.1/apps', result.appUrl);
        chrome.test.assertEq('<xml>testDescription</xml>',
                             result.deviceDescription);
        chrome.test.succeed();
      };
      chrome.dial.fetchDeviceDescription('testDevice',
                                         fetchDeviceDescriptionCallback);
    },
    function fetchDeviceDecriptionFails() {
      const fetchDeviceDescriptionCallbackFails = result => {
        chrome.test.assertEq(undefined, result);
        chrome.test.assertLastError('Device not found');
        chrome.test.succeed();
      };
      chrome.dial.fetchDeviceDescription('unknownDevice',
                                         fetchDeviceDescriptionCallbackFails);
    }
    // TODO(mfoltz): Test other fetch errors
  ]);
};
