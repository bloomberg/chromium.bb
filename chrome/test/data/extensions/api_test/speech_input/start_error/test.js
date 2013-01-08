// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extension Speech Input api test.
// browser_tests --gtest_filter="ExtensionSpeechInputApiTest.*"

chrome.test.runTests([
  function testSpeechInputStartError() {

    // Recording is not active, start it.
    chrome.experimental.speechInput.isRecording(function(recording) {
      chrome.test.assertNoLastError();
      chrome.test.assertFalse(recording);
      chrome.experimental.speechInput.start({}, started);
    });

    // Start should fail because no devices are available.
    function started() {
      chrome.test.assertEq(chrome.runtime.lastError.message,
          "noRecordingDeviceFound");

      // Recording shouldn't have started.
      chrome.experimental.speechInput.isRecording(function(recording) {
        chrome.test.assertNoLastError();
        chrome.test.assertFalse(recording);

        // Stopping should fail since we're back in the idle state.
        chrome.experimental.speechInput.stop(function() {
          chrome.test.assertEq(chrome.runtime.lastError.message,
              "invalidOperation");
          chrome.test.succeed();
        });
      });
    }
  }
]);
