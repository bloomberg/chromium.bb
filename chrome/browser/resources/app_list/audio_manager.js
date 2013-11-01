// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The manager of audio streams and interaction with the plugin.
 */

cr.define('speech', function() {
  'use strict';

  /**
   * The enum of the status of hotword audio recognition.
   *
   * @enum {number}
   */
  var AudioState = {
    UNINITIALIZED: 0,
    READY: 1,
    RECOGNIZING: 2
  };

  /**
   * @constructor
   */
  function AudioManager(onReady, onRecognizing, onRecognized) {
    this.state = AudioState.UNINITIALIZED;
    if (!speech.isPluginAvailable())
      return;
    this.onReady_ = onReady;
    this.onRecognizing_ = onRecognizing;
    this.pluginManager_ = new speech.PluginManager(
        this.onPluginReady_.bind(this), onRecognized);
    this.audioContext_ = new window.webkitAudioContext();
    this.audioProc_ = null;
    this.pluginManager_.scheduleInitialize(
        this.audioContext_.sampleRate,
        'chrome://app-list/okgoogle_hotword.config');
  };

  /**
   * Called when the plugin is ready.
   *
   * @private
   */
  AudioManager.prototype.onPluginReady_ = function() {
    this.state = AudioState.READY;
    this.onReady_();
  };

  /**
   * Called when the audio data arrives.
   *
   * @param {Event} audioEvent The audio event.
   * @private
   */
  AudioManager.prototype.onAudioProcess_ = function(audioEvent) {
    var data = audioEvent.inputBuffer.getChannelData(0);
    var intData = new Int16Array(data.length);
    for (var i = 0; i < data.length; ++i)
      intData[i] = Math.round(data[i] * 32767);
    this.pluginManager_.sendAudioData(intData.buffer);
  };

  /**
   * Called when the audio stream is ready.
   *
   * @param {MediaStream} stream The media stream which is now available.
   * @private
   */
  AudioManager.prototype.onAudioReady_ = function(stream) {
    var audioIn = this.audioContext_.createMediaStreamSource(stream);
    this.audioProc_ = this.audioContext_.createScriptProcessor(
        4096 /* buffer size */, 1 /* channels */, 1 /* channels */);
    this.audioProc_.onaudioprocess = this.onAudioProcess_.bind(this);

    audioIn.connect(this.audioProc_);
    this.audioProc_.connect(this.audioContext_.destination);
    this.state = AudioState.RECOGNIZING;
    this.onRecognizing_();
  };

  /**
   * Starts the audio recognition with the plugin.
   */
  AudioManager.prototype.start = function() {
    // Not yet initialized.
    if (this.state != AudioState.READY)
      return;
    if (this.pluginManager_.state < speech.PluginState.READY)
      return;

    if (this.pluginManager_.state == speech.PluginState.READY)
      this.pluginManager_.startRecognizer();

    if (this.audioProc_) {
      this.audioProc_.connect(this.audioContext_.destination);
      this.state = AudioState.RECOGNIZING;
      this.onRecognizing_();
      return;
    }

    navigator.webkitGetUserMedia(
        {audio: true},
        this.onAudioReady_.bind(this),
        function(msg) { console.error('Failed to getUserMedia: ' + msg); });
  };

  /**
   * Stops the audio recognition.
   */
  AudioManager.prototype.stop = function() {
    if (this.state <= AudioState.READY)
      return;
    this.audioProc_.disconnect();
    this.pluginManager_.stopRecognizer();
    this.state = AudioState.READY;
  };

  return {
    AudioManager: AudioManager
  };
});
