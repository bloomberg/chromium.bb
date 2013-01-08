// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Extension Speech Input api test.
// browser_tests --gtest_filter="ExtensionSpeechInputApiTest.*"

chrome.test.runTests([
  function testSpeechInputStartStop() {
    var count = 0;

    // Recording is not active, start it.
    chrome.experimental.speechInput.isRecording(function(recording) {
      chrome.test.assertNoLastError();
      chrome.test.assertFalse(recording);
      chrome.experimental.speechInput.start({}, started);
    });

    // There should be no error and recording should be active. Stop it.
    function started() {
      chrome.test.assertNoLastError();
      chrome.experimental.speechInput.isRecording(function(recording) {
        chrome.test.assertNoLastError();
        chrome.test.assertTrue(recording);
        chrome.experimental.speechInput.stop(stopped);
      });
    }

    // There should be no error and recording shouldn't be active again.
    // Restart recording if not repeated at least 3 times.
    function stopped() {
      chrome.test.assertNoLastError();
      chrome.experimental.speechInput.isRecording(function(recording) {
        chrome.test.assertNoLastError();
        chrome.test.assertFalse(recording);
        if (++count < 3)
          chrome.experimental.speechInput.start({}, started);
        else
          chrome.test.succeed();
      });
    }
  },

  function testSpeechInputInvalidOperation() {
    // Calling start once recording should generate an invalid operation error.
    chrome.experimental.speechInput.stop(function() {
      chrome.test.assertEq(chrome.runtime.lastError.message,
          "invalidOperation");
      chrome.experimental.speechInput.start({}, started);
    });

    // Calling stop before recording should generate an invalid operation error.
    function started() {
      chrome.test.assertNoLastError();
      chrome.experimental.speechInput.start({}, function() {
        chrome.test.assertEq(chrome.runtime.lastError.message,
            "invalidOperation");
        chrome.test.succeed();
      });
    }
  }
]);
