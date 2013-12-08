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
    READY: 'READY',
    HOTWORD_RECOGNIZING: 'HOTWORD_RECOGNIZING',
    RECOGNIZING: 'RECOGNIZING',
    STOPPING: 'STOPPING'
  };

  /**
   * @constructor
   */
  function SpeechManager() {
    this.audioManager_ = new speech.AudioManager();
    this.audioManager_.addEventListener('audio', this.onAudioLevel_.bind(this));
    if (speech.isPluginAvailable()) {
      var pluginManager = new speech.PluginManager(
          this.onHotwordRecognizerReady_.bind(this),
          this.onHotwordRecognized_.bind(this));
      pluginManager.scheduleInitialize(
          this.audioManager_.getSampleRate(),
          'chrome://app-list/okgoogle_hotword.config');
    }
    this.speechRecognitionManager_ = new speech.SpeechRecognitionManager(this);
    this.setState_(SpeechState.READY);
  }

  /**
   * Updates the state.
   *
   * @param {SpeechState} newState The new state.
   * @private
   */
  SpeechManager.prototype.setState_ = function(newState) {
    this.state = newState;
  };

  /**
   * Called with the mean audio level when audio data arrives.
   *
   * @param {cr.event.Event} event The event object for the audio data.
   * @private
   */
  SpeechManager.prototype.onAudioLevel_ = function(event) {
    var data = event.data;
    var level = 0;
    for (var i = 0; i < data.length; ++i)
      level += Math.abs(data[i]);
    level /= data.length;
    chrome.send('speechSoundLevel', [level]);
  };

  /**
   * Called when the hotword recognizer is ready.
   *
   * @param {PluginManager} pluginManager The hotword plugin manager which gets
   *   ready.
   * @private
   */
  SpeechManager.prototype.onHotwordRecognizerReady_ = function(pluginManager) {
    this.pluginManager_ = pluginManager;
    this.audioManager_.addEventListener(
        'audio', pluginManager.sendAudioData.bind(pluginManager));
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
    this.setState_(SpeechState.READY);
    this.pluginManager_.stopRecognizer();
    this.speechRecognitionManager_.start();
  };

  /**
   * Called when the speech recognition has happened.
   *
   * @param {string} result The speech recognition result.
   * @param {boolean} isFinal Whether the result is final or not.
   */
  SpeechManager.prototype.onSpeechRecognized = function(result, isFinal) {
    chrome.send('speechResult', [result, isFinal]);
    if (isFinal)
      this.speechRecognitionManager_.stop();
  };

  /**
   * Called when the speech recognition has started.
   */
  SpeechManager.prototype.onSpeechRecognitionStarted = function() {
    this.setState_(SpeechState.RECOGNIZING);
    chrome.send('setSpeechRecognitionState', ['on']);
  };

  /**
   * Called when the speech recognition has ended.
   */
  SpeechManager.prototype.onSpeechRecognitionEnded = function() {
    // Restarts the hotword recognition.
    if (this.state != SpeechState.STOPPING && this.pluginManager_) {
      this.pluginManager_.startRecognizer();
      this.audioManager_.start();
      this.setState_(SpeechState.HOTWORD_RECOGNIZING);
    } else {
      this.audioManager_.stop();
    }
    chrome.send('setSpeechRecognitionState', ['off']);
  };

  /**
   * Called when a speech has started.
   */
  SpeechManager.prototype.onSpeechStarted = function() {
    if (this.state == SpeechState.RECOGNIZING)
      chrome.send('setSpeechRecognitionState', ['in-speech']);
  };

  /**
   * Called when a speech has ended.
   */
  SpeechManager.prototype.onSpeechEnded = function() {
    if (this.state == SpeechState.RECOGNIZING)
      chrome.send('setSpeechRecognitionState', ['on']);
  };

  /**
   * Called when an error happened during the speech recognition.
   *
   * @param {SpeechRecognitionError} e The error object.
   */
  SpeechManager.prototype.onSpeechRecognitionError = function(e) {
    if (this.state != SpeechState.STOPPING)
      this.setState_(SpeechState.READY);
  };

  /**
   * Starts the speech recognition session.
   */
  SpeechManager.prototype.start = function() {
    if (!this.pluginManager_)
      return;

    if (this.state == SpeechState.HOTWORD_RECOGNIZING) {
      console.warn('Already in recognition state...');
      return;
    }

    this.pluginManager_.startRecognizer();
    this.audioManager_.start();
    this.setState_(SpeechState.HOTWORD_RECOGNIZING);
  };

  /**
   * Stops the speech recognition session.
   */
  SpeechManager.prototype.stop = function() {
    if (this.pluginManager_)
      this.pluginManager_.stopRecognizer();

    // SpeechRecognition is asynchronous.
    this.audioManager_.stop();
    if (this.state == SpeechState.RECOGNIZING) {
      this.setState_(SpeechState.STOPPING);
      this.speechRecognitionManager_.stop();
    } else {
      this.setState_(SpeechState.READY);
    }
  };

  /**
   * Toggles the current state of speech recognition.
   */
  SpeechManager.prototype.toggleSpeechRecognition = function() {
    if (this.state == SpeechState.RECOGNIZING) {
      this.audioManager_.stop();
      this.speechRecognitionManager_.stop();
    } else {
      if (this.pluginManager_)
        this.pluginManager_.stopRecognizer();
      if (this.audioManager_.state == speech.AudioState.STOPPED)
        this.audioManager_.start();
      this.speechRecognitionManager_.start();
    }
  };

  return {
    SpeechManager: SpeechManager
  };
});
