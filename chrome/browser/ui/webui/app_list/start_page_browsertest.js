// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for kiosk app settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function AppListStartPageWebUITest() {}

/**
 * Mock of audioContext.
 * @constructor
 */
function mockAudioContext() {
  this.sampleRate = 44100; /* some dummy number */
}

mockAudioContext.prototype = {
  createMediaStreamSource: function(stream) {
    return {connect: function(audioProc) {},
            disconnect: function() {}};
  },
  createScriptProcessor: function(bufSize, in_channels, out_channels) {
    return {connect: function(destination) {},
            disconnect: function() {}};
  }
};

AppListStartPageWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browser to app launcher start page.
   */
  browsePreload: 'chrome://app-list/',

  /**
   * Placeholder for mock speech recognizer.
   */
  speechRecognizer: null,

  /**
   * Sends the speech recognition result.
   *
   * @param {string} result The testing result.
   * @param {boolean} isFinal Whether the result is final or not.
   */
  sendSpeechResult: function(result, isFinal) {
    var speechEvent = new Event('test');
    // Each result contains a list of alternatives and 'isFinal' flag.
    var speechResult = [{transcript: result}];
    speechResult.isFinal = isFinal;
    speechEvent.results = [speechResult];
    this.speechRecognizer.onresult(speechEvent);
  },

  /**
   * Registers the webkitSpeechRecognition mock for test.
   * @private
   */
  registerMockSpeechRecognition_: function() {
    var owner = this;
    function mockSpeechRecognition() {
      this.inSpeech_ = false;
      owner.speechRecognizer = this;
    }

    mockSpeechRecognition.prototype = {
      start: function() {
        this.onstart();
      },

      abort: function() {
        if (this.inSpeech_)
          this.onspeechend();
        this.onerror(new Error());
        this.onend();
      }
    },

    window.webkitSpeechRecognition = mockSpeechRecognition;
  },

  /**
   * Mock of webkitGetUserMedia for start page.
   *
   * @private
   * @param {object} constraint The constraint parameter.
   * @param {Function} success The success callback.
   * @param {Function} error The error callback.
   */
  mockGetUserMedia_: function(constraint, success, error) {
    function getAudioTracks() {
    }
    assertTrue(constraint.audio);
    assertNotEquals(null, error, 'error callback must not be null');
    var audioTracks = [];
    for (var i = 0; i < 2; ++i) {
      audioTracks.push(this.audioTrackMocks[i].proxy());
    }
    success({getAudioTracks: function() { return audioTracks; }});
  },

  /** @override */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['initialize',
                                     'launchApp',
                                     'setSpeechRecognitionState',
                                     'speechResult']);
    this.mockHandler.stubs().initialize();
    this.mockHandler.stubs().launchApp(ANYTHING);

    this.registerMockSpeechRecognition_();
    window.AudioContext = mockAudioContext;
    navigator.webkitGetUserMedia = this.mockGetUserMedia_.bind(this);
    this.audioTrackMocks = [mock(MediaStreamTrack), mock(MediaStreamTrack)];
  }
};

TEST_F('AppListStartPageWebUITest', 'Basic', function() {
  assertEquals(this.browsePreload, document.location.href);
});

TEST_F('AppListStartPageWebUITest', 'LoadDoodle', function() {
  var doodleData = {
    'ddljson': {
      'transparent_large_image': {
        'url': 'doodle.png'
      },
      'alt_text': 'Doodle alt text',
      'target_url': '/target.html'
    }
  };

  assertEquals(null, $('doodle'));

  // Load the doodle with a target url and alt text.
  appList.startPage.onAppListDoodleUpdated(doodleData,
                                           'http://example.com/x/');
  assertNotEquals(null, $('doodle'));
  assertEquals('http://example.com/x/doodle.png', $('doodle_image').src);
  assertEquals(doodleData.ddljson.alt_text, $('doodle_image').title);
  assertEquals('http://example.com/target.html', $('doodle_link').href);

  // Reload the doodle without a target url and alt text.
  doodleData.ddljson.alt_text = undefined;
  doodleData.ddljson.target_url = undefined;
  appList.startPage.onAppListDoodleUpdated(doodleData,
                                           'http://example.com/x/');
  assertNotEquals(null, $('doodle'));
  assertEquals('http://example.com/x/doodle.png', $('doodle_image').src);
  assertEquals('', $('doodle_image').title);
  assertEquals(null, $('doodle_link'));


  appList.startPage.onAppListDoodleUpdated({},
                                           'http://example.com/');
  assertEquals(null, $('doodle'));
});
