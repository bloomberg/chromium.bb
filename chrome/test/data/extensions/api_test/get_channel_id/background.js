// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetChannelId() {
  chrome.experimental.pushMessaging.getChannelId(
      chrome.test.callbackFail('3'));
}

chrome.test.runTests([testGetChannelId]);
