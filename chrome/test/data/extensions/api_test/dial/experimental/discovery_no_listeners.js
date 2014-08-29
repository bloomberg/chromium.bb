// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function discoverNowWithoutListeners() {
      var discoverNowCallback = function(result) {
        chrome.test.assertNoLastError();
        chrome.test.assertTrue(typeof(result) == 'boolean');
        if (!result) {
          chrome.test.succeed();
        } else {
          chrome.test.fail();
        }
      };
      chrome.dial.discoverNow(discoverNowCallback);
    }
  ]);
};