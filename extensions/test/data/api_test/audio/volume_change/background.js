// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function waitForLevelChangedEventTests() {
    chrome.test.listenOnce(chrome.audio.OnLevelChanged, function (id, level) {
      chrome.test.assertEq('30001', id);
      chrome.test.assertEq(60, level);
    });
  }
]);

chrome.test.sendMessage('loaded');
