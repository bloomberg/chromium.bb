// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifyDetails(details) {
  chrome.test.assertEq(1, details.subchannelId);
  chrome.test.assertEq('payload', details.payload);
}

function testEventDispatch() {
  chrome.pushMessaging.onMessage.addListener(
      chrome.test.callbackPass(verifyDetails));
}

chrome.test.runTests([testEventDispatch]);
