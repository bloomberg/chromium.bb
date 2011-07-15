// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="ExtensionApiTest.TtsChromeOs"

chrome.test.runTests([
  function testChromeOsSpeech() {
    var callbacks = 0;
    chrome.experimental.tts.speak(
        'text 1',
        {
         'onEvent': function(event) {
           callbacks++;
           chrome.test.assertEq('interrupted', event.type);
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
    chrome.experimental.tts.speak(
        'text 2',
        {
         'onEvent': function(event) {
           chrome.test.assertEq('end', event.type);
           callbacks++;
           if (callbacks == 2) {
             chrome.test.succeed();
           } else {
             chrome.test.fail();
           }
         }
        },
        function() {
          chrome.test.assertNoLastError();
        });
  }

]);
