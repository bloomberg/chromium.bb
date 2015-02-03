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
   * Sample doodle data.
   */
  doodleData_: {
    'ddljson': {
      'transparent_large_image': {
        'url': 'doodle.png'
      }
    }
  },

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
  var doodle = $('doodle');
  assertEquals('', doodle.src);
  appList.startPage.onAppListDoodleUpdated(this.doodleData_,
                                           'http://example.com/');
  assertEquals('http://example.com/doodle.png', doodle.src);
});

TEST_F('AppListStartPageWebUITest', 'SpeechRecognitionState', function() {
  this.mockHandler.expects(once()).setSpeechRecognitionState('READY');
  appList.startPage.onAppListShown();
  this.mockHandler.expects(once()).setSpeechRecognitionState('RECOGNIZING');
  appList.startPage.toggleSpeechRecognition();
  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  this.mockHandler.expects(once()).setSpeechRecognitionState('READY');
  for (var i = 0; i < this.audioTrackMocks.length; ++i) {
    this.audioTrackMocks[i].expects(once()).stop();
  }
  appList.startPage.toggleSpeechRecognition();
  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  this.mockHandler.expects(once()).setSpeechRecognitionState('RECOGNIZING');
  appList.startPage.toggleSpeechRecognition();
  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  this.mockHandler.expects(once()).setSpeechRecognitionState('STOPPING');
  this.mockHandler.expects(once()).setSpeechRecognitionState('READY');
  for (var i = 0; i < this.audioTrackMocks.length; ++i) {
    this.audioTrackMocks[i].expects(once()).stop();
  }
  appList.startPage.onAppListHidden();
});

TEST_F('AppListStartPageWebUITest', 'SpeechRecognition', function() {
  this.mockHandler.expects(once()).setSpeechRecognitionState('READY');
  appList.startPage.onAppListShown();
  this.mockHandler.expects(once()).setSpeechRecognitionState('RECOGNIZING');
  appList.startPage.toggleSpeechRecognition();
  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  this.mockHandler.expects(once()).setSpeechRecognitionState('IN_SPEECH');
  this.speechRecognizer.onspeechstart();
  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  this.mockHandler.expects(once()).speechResult('test,false');
  this.sendSpeechResult('test', false);
  Mock4JS.verifyAllMocks();
  Mock4JS.clearMocksToVerify();

  this.mockHandler.expects(once()).speechResult('test,true');
  this.mockHandler.expects(once()).setSpeechRecognitionState('READY');
  for (var i = 0; i < this.audioTrackMocks.length; ++i) {
    this.audioTrackMocks[i].expects(once()).stop();
  }
  this.sendSpeechResult('test', true);
});
