// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

chrome.test.runTests([
  function testSpeakCallbackFunctionIsCalled() {
    chrome.experimental.tts.speak('hello world', {}, function() {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
  }
]);
