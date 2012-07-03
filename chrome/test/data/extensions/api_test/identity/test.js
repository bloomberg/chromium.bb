// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var pass = chrome.test.callbackPass;

chrome.test.runTests([
  function getAuthToken() {
    chrome.experimental.identity.getAuthToken(
        {},
        pass(function(token) {
      assertEq('auth_token', token);
    }));
  },

  function launchAuthFlow() {
    chrome.experimental.identity.launchWebAuthFlow(
        {url: 'https://some/url'},
        pass(function(url) {
      assertEq('https://abcd.chromiumapp.org/cb#access_token=tok', url);
    }));
  }
]);
