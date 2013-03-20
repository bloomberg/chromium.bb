// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function channelIdCallback(message) {
  chrome.test.assertEq(message.channelId, "");
}

function testGetChannelId() {
  // the api call should fail because no user is signed in
  chrome.pushMessaging.getChannelId(
      false, chrome.test.callbackFail("The user is not signed in.",
                                      channelIdCallback));
}

chrome.test.runTests([testGetChannelId]);
