// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * A mock text-to-speech engine for tests.
 * This class has functions and callbacks necessary for Select-to-Speak
 * to function. It keeps track of the utterances currently being spoken,
 * and whether TTS should be speaking or is stopped.
 */
var MockTts = function() {
  /**
   * @type {Array<String>}
   * @private
   */
  this.pendingUtterances_ = [];

  /**
   * @type {boolean}
   * @private
   */
  this.currentlySpeaking_ = false;
};

MockTts.prototype = {
  // Functions based on methods in
  // https://developer.chrome.com/extensions/tts
  speak: function(utterance, options) {
    this.pendingUtterances_.push(utterance);
    this.currentlySpeaking_ = true;
  },
  stop: function() {
    this.pendingUtterances_ = [];
    this.currentlySpeaking_ = false;
  },
  getVoices: function(callback) {
    callback([{voiceName: 'English US', lang: 'English'}]);
  },
  isSpeaking: function(callback) {
    callback(this.currentlySpeaking_);
  },
  // Functions for testing
  currentlySpeaking: function() {
    return this.currentlySpeaking_;
  },
  pendingUtterances: function() {
    return this.pendingUtterances_;
  }
};
