// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The class to Manage both offline / online speech recognition.
 */

<include src="plugin_manager.js"/>
<include src="audio_manager.js"/>
<include src="speech_recognition_manager.js"/>

cr.define('speech', function() {
  'use strict';

  /**
   * The state of speech recognition.
   *
   * @enum {string}
   */
  var SpeechState = {
    UNINITIALIZED: 'UNINITIALIZED',
    READY: 'READY',
    HOTWORD_RECOGNIZING: 'HOTWORD_RECOGNIZING',
    RECOGNIZING: 'RECOGNIZING'
  };

  /**
   * @constructor
   */
  function SpeechManager() {
    this.audioManager_ = new speech.AudioManager(
        this.onHotwordRecognizerReady_.bind(this),
        this.onHotwordRecognizing_.bind(this),
        this.onHotwordRecognized_.bind(this));
    this.speechRecognitionManager_ = new speech.SpeechRecognitionManager(this);
    this.setState_(SpeechState.UNINITIALIZED);
  }

  /**
   * Updates the state.
   *
   * @param {SpeechState} newState The new state.
   * @private
   */
  SpeechManager.prototype.setState_ = function(newState) {
    this.state = newState;
    console.log('speech state: ' + newState);
  };

  /**
   * Called when the hotword recognizer is ready.
   *
   * @private
   */
  SpeechManager.prototype.onHotwordRecognizerReady_ = function() {
    this.setState_(SpeechState.READY);
  };

  /**
   * Called when the hotword is recognized.
   *
   * @param {number} confidence The confidence store of the recognition.
   * @private
   */
  SpeechManager.prototype.onHotwordRecognized_ = function(confidence) {
    if (this.state != SpeechState.HOTWORD_RECOGNIZING)
      return;
    this.audioManager_.stop();
    this.setState_(SpeechState.READY);
    this.speechRecognitionManager_.start();
  };

  /**
   * Called when the hotword recognition has started.
   *
   * @private
   */
  SpeechManager.prototype.onHotwordRecognizing_ = function() {
    this.setState_(SpeechState.HOTWORD_RECOGNIZING);
  };

  /**
   * Called when the speech recognition has happened.
   *
   * @param {string} result The speech recognition result.
   * @param {boolean} isFinal Whether the result is final or not.
   */
  SpeechManager.prototype.onSpeechRecognized = function(result, isFinal) {
    console.log('speech result: ' + result + ' ' +
        (isFinal ? 'final' : 'interim'));
    if (isFinal) {
      chrome.send('search', [result]);
      this.speechRecognitionManager_.stop();
    }
  };

  /**
   * Called when the speech recognition has started.
   */
  SpeechManager.prototype.onSpeechRecognitionStarted = function() {
    this.setState_(SpeechState.RECOGNIZING);
  };

  /**
   * Called when the speech recognition has ended.
   */
  SpeechManager.prototype.onSpeechRecognitionEnded = function() {
    // Restarts the hotword recognition.
    this.audioManager_.start();
  };

  /**
   * Called when an error happened during the speech recognition.
   *
   * @param {Error} e The error object.
   */
  SpeechManager.prototype.onSpeechRecognitionError = function(e) {
    this.setState_(SpeechState.UNINITIALIZED);
  };

  /**
   * Starts the speech recognition session.
   */
  SpeechManager.prototype.start = function() {
    if (this.state == SpeechState.UNINITIALIZED) {
      console.warn('hotword recognizer is not yet initialized');
      return;
    }

    if (this.state != SpeechState.READY) {
      console.warn('Already in recognition state...');
      return;
    }

    this.audioManager_.start();
  };

  /**
   * Stops the speech recognition session.
   */
  SpeechManager.prototype.stop = function() {
    if (this.state == SpeechState.UNINITIALIZED)
      return;

    this.audioManager_.stop();
    this.speechRecognitionManager_.stop();
    this.setState_(SpeechState.READY);
  };

  return {
    SpeechManager: SpeechManager
  };
});
