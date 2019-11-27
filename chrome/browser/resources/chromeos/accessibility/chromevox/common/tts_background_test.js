// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_e2e_test_base.js', '../testing/assert_additions.js'
]);

// E2E tests for TtsBackground.

/**
 * Test fixture.
 * @constructor
 * @extends {ChromeVoxE2ETest}
 */
function ChromeVoxTtsBackgroundTest() {
  ChromeVoxE2ETest.call(this);
}

ChromeVoxTtsBackgroundTest.prototype = {
  __proto__: ChromeVoxE2ETest.prototype
};

SYNC_TEST_F('ChromeVoxTtsBackgroundTest', 'Preprocess', function() {
  var tts = new TtsBackground(false);
  var preprocess = tts.preprocess.bind(tts);

  // Punctuation.
  assertEquals('dot', preprocess('.'));
  assertEquals('x.', preprocess('x.'));
  assertEquals('.x', preprocess('.x'));
  assertEquals('space', preprocess(' '));
  assertEquals('', preprocess('  '));
  assertEquals('A', preprocess('a'));
  assertEquals('A', preprocess('A'));
  assertEquals('a.', preprocess('a.'));
  assertEquals('.a', preprocess('.a'));

  assertEquals('10 equal signs', preprocess('=========='));

  assertEquals('new line', preprocess('\n'));
  assertEquals('return', preprocess('\r'));

  assertEquals('bullet 2 bullets', preprocess('\u2022 \u2022\u2022'));
});

TEST_F('ChromeVoxTtsBackgroundTest', 'UpdateVoice', function() {
  var tts = new TtsBackground(false);
  var voices = [
    {lang: 'zh-CN', voiceName: 'Chinese'},
    {lang: 'zh-TW', voiceName: 'Chinese (Taiwan)'},
    {lang: 'es', voiceName: 'Spanish'},
    {lang: 'en-US', voiceName: 'U.S. English'}
  ];

  chrome.tts.getVoices = function(callback) {
    callback(voices);
  };

  // Asks this test to process the next task immediately.
  var flushNextTask = function() {
    var task = tasks.shift();
    if (!task) {
      return;
    }

    if (task.setup) {
      task.setup();
    }
    tts.updateVoice_(task.testVoice, this.newCallback(function(actualVoice) {
      assertEquals(task.expectedVoice, actualVoice);
      flushNextTask();
    }));
  }.bind(this);

  assertTrue(!tts.currentVoice);

  var tasks = [
    {testVoice: '', expectedVoice: constants.SYSTEM_VOICE},

    {
      setup: function() {
        voices[3].lang = 'en';
      },
      testVoice: 'U.S. English',
      expectedVoice: 'U.S. English'
    },

    {
      setup: function() {
        voices[3].lang = 'fr-FR';
        voices[3].voiceName = 'French';
      },
      testVoice: '',
      expectedVoice: constants.SYSTEM_VOICE
    },

    {testVoice: 'French', expectedVoice: 'French'},

    {testVoice: 'NotFound', expectedVoice: constants.SYSTEM_VOICE},
  ];

  flushNextTask();
});

// This test only works if Google tts is installed. Run it locally.
TEST_F(
    'ChromeVoxTtsBackgroundTest', 'DISABLED_EmptyStringCallsCallbacks',
    function() {
      var tts = new TtsBackground(false);
      var startCalls = 0, endCalls = 0;
      assertCallsCallbacks = function(text, speakCalls) {
        tts.speak(text, QueueMode.QUEUE, {
          startCallback: function() {
            ++startCalls;
          },
          endCallback: this.newCallback(function() {
            ++endCalls;
            assertEquals(speakCalls, endCalls);
            assertEquals(endCalls, startCalls);
          })
        });
      }.bind(this);

      assertCallsCallbacks('', 1);
      assertCallsCallbacks('  ', 2);
      assertCallsCallbacks(' \u00a0 ', 3);
    });

SYNC_TEST_F(
    'ChromeVoxTtsBackgroundTest', 'CapitalizeSingleLettersAfterNumbers',
    function() {
      var tts = new TtsBackground(false);
      var preprocess = tts.preprocess.bind(tts);

      // Capitalize single letters if they appear directly after a number.
      assertEquals(
          'Click to join the 5G network',
          preprocess('Click to join the 5g network'));
      assertEquals(
          'I ran a 100M sprint in 10 S',
          preprocess('I ran a 100m sprint in 10 s'));
      // Do not capitalize the letter "a".
      assertEquals(
          'Please do the shopping at 3 a thing came up at work',
          preprocess('Please do the shopping at 3 a thing came up at work'));
    });
