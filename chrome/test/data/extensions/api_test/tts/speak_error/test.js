// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testSpeakCallbackFunctionIsCalled() {
    var callbacks = 0;
    chrome.experimental.tts.speak('first try', {'enqueue': true}, function() {
        chrome.test.assertNoLastError();
        callbacks++;
      });
    chrome.experimental.tts.speak('second try', {'enqueue': true}, function() {
        chrome.test.assertEq('epic fail', chrome.extension.lastError.message);
        callbacks++;
      });
    chrome.experimental.tts.speak('third try', {'enqueue': true}, function() {
        chrome.test.assertNoLastError();
        callbacks++;
        if (callbacks == 3) {
          chrome.test.succeed();
        } else {
          chrome.test.fail();
        }
      });
  }
]);
