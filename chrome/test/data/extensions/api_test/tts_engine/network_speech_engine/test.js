// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testNetworkSpeechVoices() {
    chrome.tts.getVoices(function(voices) {
      chrome.test.assertTrue(voices.length >= 6);
      for (var i = 0; i < voices.length; i++) {
        chrome.test.assertEq(true, voices[i].remote);
      }
      chrome.test.succeed();
    });
  }
]);
