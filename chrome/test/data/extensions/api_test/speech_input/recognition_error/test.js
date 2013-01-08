// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extension Speech Input api test.
// browser_tests --gtest_filter="ExtensionSpeechInputApiTest.*"

chrome.test.runTests([
  function testSpeechInputRecognitionError() {
    // Results should never be provided in this test case.
    chrome.experimental.speechInput.onResult.addListener(function(event) {
      chrome.test.fail();
    });

    // Ensure the recognition error happens.
    chrome.experimental.speechInput.onError.addListener(function(error) {
      chrome.test.assertEq(error.code, "networkError");

      // No recording should be happening after an error.
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

    // Start recording.
    chrome.experimental.speechInput.start({}, function() {
      chrome.test.assertNoLastError();
    });
  }
]);
