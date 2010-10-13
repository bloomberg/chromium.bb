// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter=ExtensionApiTest.TtsOnChromeOs

chrome.test.runTests([
  function testSpeak() {
    chrome.experimental.tts.speak('hello world', {}, function() {
          chrome.test.succeed();
        });
  },

  function testStop() {
    chrome.experimental.tts.stop();
    chrome.test.succeed();
  },

  function testIsSpeaking() {
    for (var i = 0; i < 3; i++) {
      chrome.experimental.tts.isSpeaking(function(speaking) {
          chrome.test.assertTrue(speaking);
        });
    }
    chrome.experimental.tts.isSpeaking(function(speaking) {
        chrome.test.assertFalse(speaking);
        chrome.test.succeed();
      });
  }

]);
