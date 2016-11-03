// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function waitForMuteChangedEventTests() {
    chrome.test.listenOnce(
        chrome.audio.OnMuteChanged,
        function(isInput, isMuted) {
          chrome.test.assertFalse(isInput);
          chrome.test.assertFalse(isMuted);
        });
  }
]);

chrome.test.sendMessage('loaded');
