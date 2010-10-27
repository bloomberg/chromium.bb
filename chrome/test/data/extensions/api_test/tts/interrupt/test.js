// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testAllSpeakCallbackFunctionsAreCalled() {
    var callbacks = 0;
    chrome.experimental.tts.speak('text 1', {'enqueue': false}, function() {
        chrome.test.assertNoLastError();
        callbacks++;
      });
    chrome.experimental.tts.speak('text 2', {'enqueue': false}, function() {
        chrome.test.assertNoLastError();
        callbacks++;
        if (callbacks == 2) {
          chrome.test.succeed();
        } else {
          chrome.test.fail();
        }
      });
  }
]);
