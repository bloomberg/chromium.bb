// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function successfulSetCreds() {
    var userEmail = 'foo@gmail.com';
    var robotEmail = 'foorobot@googleusercontent.com';
    var credentials = '1/23546efa54';
    chrome.chromeAuthPrivate.setCloudPrintCredentials(
        userEmail, robotEmail, credentials,
        chrome.test.callbackPass(function(result) {
           // In test mode, we expect the API to reflect the arguments back to
           // us appended together.
           chrome.test.assertNoLastError();
           chrome.test.assertEq(result, userEmail + robotEmail + credentials);
         }));
  }
];

chrome.test.runTests(tests);
