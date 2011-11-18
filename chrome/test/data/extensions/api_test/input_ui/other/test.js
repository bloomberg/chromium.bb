// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// experimental.input.ui API test for Chrome
// browser_tests --gtest_filter=ExtensionApiTest.InputUI

chrome.test.runTests([
  function inputUINotExist() {
    if (!chrome.experimental.input.ui) {
      chrome.test.succeed();
    } else {
      chrome.test.fail();
    }
  }
]);
