// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TTS api test for Chrome on ChromeOS.
// browser_tests.exe --gtest_filter="TtsApiTest.*"

if (!chrome.tts) {
  chrome.tts = chrome.experimental.tts;
}

chrome.test.runTests([
  function testNoListeners() {
    // This call should go to native speech because we haven't registered
    // any listeners.
    chrome.tts.speak('native speech', {}, function() {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
  },
  function testTtsProvider() {
    // Register listeners for speech functions, enabling this extension
    // to be a TTS provider.
    var speakListener = function(utterance, options, callback) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq('extension speech', utterance);
        callback();
      };
    var stopListener = function() {};
    chrome.tts.onSpeak.addListener(speakListener);
    chrome.tts.onStop.addListener(stopListener);

    // This call should go to our own speech provider.
    chrome.tts.speak('extension speech', {}, function() {
        chrome.test.assertNoLastError();
        chrome.tts.onSpeak.removeListener(speakListener);
        chrome.tts.onStop.removeListener(stopListener);
        chrome.test.succeed();
      });
  },
  function testVoiceMatching() {
    // Count the number of times our callback functions have been called.
    var callbacks = 0;
    // Count the number of times our TTS provider has been called.
    var speakListenerCalls = 0;

    // Register listeners for speech functions.
    var speakListener = function(utterance, options, callback) {
        speakListenerCalls++;
        callback();
      };
    var stopListener = function() {};
    chrome.tts.onSpeak.addListener(speakListener);
    chrome.tts.onStop.addListener(stopListener);

    // These don't match the voices in the manifest, so they should
    // go to native speech. The gmock assertions in TtsApiTest::Provide
    // enforce that the native TTS handlers are called.
    chrome.tts.speak('native speech 2',
                     {'voiceName': 'George', 'enqueue': true}, function() {
        chrome.test.assertNoLastError();
        callbacks++;
      });
    chrome.tts.speak('native speech 3',
                     {'locale': 'fr-FR', 'enqueue': true},
                     function() {
        chrome.test.assertNoLastError();
        callbacks++;
      });

    // These do match the voices in the manifest, so they should go to our
    // own TTS provider.
    chrome.tts.speak('extension speech 2',
                     {'voiceName': 'Alice', 'enqueue': true},
                     function() {
        chrome.test.assertNoLastError();
        callbacks++;
        chrome.test.succeed();
      });
    chrome.tts.speak('extension speech 3',
                     {'voiceName': 'Pat', 'gender': 'male', 'enqueue': true},
                     function() {
        chrome.test.assertNoLastError();
        callbacks++;
        chrome.tts.onSpeak.removeListener(speakListener);
        chrome.tts.onStop.removeListener(stopListener);
        if (callbacks == 4 && speakListenerCalls == 2) {
          chrome.test.succeed();
        }
      });
  },
  function testTtsProviderError() {
    // Register listeners for speech functions, but have speak return an
    // error when it's used.
    var speakListener = function(utterance, options, callback) {
        chrome.test.assertEq('extension speech 4', utterance);
        callback('extension tts error');
      };
    var stopListener = function() {};
    chrome.tts.onSpeak.addListener(speakListener);
    chrome.tts.onStop.addListener(stopListener);

    // This should go to our own TTS provider, and we can check that we
    // get the error message.
    chrome.tts.speak('extension speech 4', {}, function() {
        chrome.test.assertEq('extension tts error',
                             chrome.extension.lastError.message);
        chrome.tts.onSpeak.removeListener(speakListener);
        chrome.tts.onStop.removeListener(stopListener);
        chrome.test.succeed();
      });
  }
]);
