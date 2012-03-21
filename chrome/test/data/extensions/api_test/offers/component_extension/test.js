// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function offersTest() {
    chrome.offersPrivate.confirmEligibility(
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result);
    }));
  }
]);
