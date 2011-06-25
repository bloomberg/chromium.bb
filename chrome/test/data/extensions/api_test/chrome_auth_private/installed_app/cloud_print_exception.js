// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is invoked from a non-component app. So this call should throw an
// exception when we try and access the chromeAuthPrivate API.
var tests = [
  function exceptionSetCreds() {
    var expectedException =
        "Error: You do not have permission to use " +
        "'chromeAuthPrivate.setCloudPrintCredentials'. Be sure to declare in " +
        "your manifest what permissions you need.";
    var userEmail = 'foo@gmail.com';
    var robotEmail = 'foorobot@googleusercontent.com';
    var credentials = '1/23546efa54';
    try {
      chrome.chromeAuthPrivate.setCloudPrintCredentials(
          userEmail, robotEmail, credentials);
    } catch (err) {
      chrome.test.assertEq(expectedException, err.toString());
      chrome.test.succeed();
      return;
    }
    chrome.test.fail();
  }
];

chrome.test.runTests(tests);
