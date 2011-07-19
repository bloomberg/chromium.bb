// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is invoked from an installed component app. Since we have an
// explicit URL check in the API, this should fail.
var tests = [
  function failureSetCreds() {
    var expectedError = "Cannot call this API from a non-cloudprint URL."
    var userEmail = 'foo@gmail.com';
    var robotEmail = 'foorobot@googleusercontent.com';
    var credentials = '1/23546efa54';
    chrome.chromeAuthPrivate.setCloudPrintCredentials(
        userEmail, robotEmail, credentials,
        chrome.test.callbackFail(expectedError));
  }
];

chrome.test.runTests(tests);
