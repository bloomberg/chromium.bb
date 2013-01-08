// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extension Speech Input api test.
// browser_tests --gtest_filter="ExtensionSpeechInputApiTest.*"

chrome.test.runTests([
  function testSpeechInputRecognition() {
    var did_sound_start = false;
    var did_sound_end = false;

    // Check that sound starts once and before ending.
    chrome.experimental.speechInput.onSoundStart.addListener(function() {
      chrome.test.assertFalse(did_sound_start);
      chrome.test.assertFalse(did_sound_end);
      did_sound_start = true;
    });

    // Check that sounds ends once and after starting.
    chrome.experimental.speechInput.onSoundEnd.addListener(function() {
      chrome.test.assertTrue(did_sound_start);
      chrome.test.assertFalse(did_sound_end);
      did_end_sound = true;
    });

    // Check that both sound events happened and that the given results
    // match the expected ones.
    chrome.experimental.speechInput.onResult.addListener(function(event) {
      chrome.test.assertTrue(did_sound_start);
      chrome.test.assertTrue(did_end_sound);
      chrome.test.assertEq(event.hypotheses.length, 1);
      chrome.test.assertEq(event.hypotheses[0].utterance, "this is a test");
      chrome.test.assertEq(event.hypotheses[0].confidence, 0.99);

      // Ensure no recording is happening after delivering results.
      chrome.experimental.speechInput.isRecording(function(recording) {
        chrome.test.assertNoLastError();
        chrome.test.assertFalse(recording);

        // Stopping should fail since we're in the idle state again.
        chrome.experimental.speechInput.stop(function() {
          chrome.test.assertEq(chrome.runtime.lastError.message,
              "invalidOperation");
          chrome.test.succeed();
        });
      });
    });

    // Ensure that no errors happened during the recognition.
    chrome.experimental.speechInput.onError.addListener(function(event) {
      chrome.test.fail();
    });

    // Recording is not active, start it.
    chrome.experimental.speechInput.isRecording(function(recording) {
      chrome.test.assertNoLastError();
      chrome.test.assertFalse(recording);
      chrome.experimental.speechInput.start({}, function() {
        chrome.test.assertNoLastError();
      });
    });
  }
]);
