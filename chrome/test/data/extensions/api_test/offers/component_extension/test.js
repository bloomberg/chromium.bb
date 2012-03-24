// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getCouponCodeTest() {
    var expected_code = "deadbeef";
    chrome.offersPrivate.getCouponCode("COUPON_CODE",
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == expected_code);
    }));
    chrome.offersPrivate.getCouponCode("GROUP_CODE",
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == expected_code);
    }));
    chrome.offersPrivate.getCouponCode("INVALID_CODE",
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == "");
    }));
  }
]);
