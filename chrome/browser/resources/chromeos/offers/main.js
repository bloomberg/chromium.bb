// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function couponCodeCallback(result) {
  response = "getCouponCode returned: " + result;
  console.log(response);
}

function consentCallback(result) {
  if (result == true) {
    chrome.offersPrivate.getCouponCode("COUPON_CODE", couponCodeCallback);
    return;
  }
  console.log("User declined ECHO!");
}

window.onload = function() {
  chrome.offersPrivate.getUserConsent(consentCallback);
}
