// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The manager of offline hotword speech recognizer plugin.
 */

cr.define('speech', function() {
  'use strict';

  /**
   * The type of the plugin state.
   ** @enum {number}
   */
  var PluginState = {
    UNINITIALIZED: 0,
    LOADED: 1,
    SAMPLING_RATE_READY: 2,
    READY: 3,
    RECOGNIZING: 4
  };

  /**
   * The command names of the plugin.
   * @enum {string}
   */
  var pluginCommands = {
    SET_SAMPLING_RATE: 'h',
    SET_CONFIG: 'm',
    START_RECOGNIZING: 'r',
    STOP_RECOGNIZING: 's'
  };

  /**
   * The regexp pattern of the hotword recognition result.
   */
  var recognitionPattern = /^HotwordFiredEvent: \[(.*)\] confidence: (.*)/;

  /**
   * Checks the availability of the plugin.
   * @return {boolean} True only if the plugin is available.
   */
  function isPluginAvailable() {
    return !!($('recognizer') && $('recognizer').postMessage);
  }

  /**
   * @constructor
   */
  function PluginManager(onReady, onRecognized) {
    this.state = PluginState.UNINITIALIZED;
    this.onReady_ = onReady;
    this.onRecognized_ = onRecognized;
    this.samplingRate_ = null;
    this.config_ = null;
    if (isPluginAvailable()) {
      $('recognizer').addEventListener('message', this.onMessage_.bind(this));
      $('recognizer').addEventListener('load', this.onLoad_.bind(this));
    }
  };

  /**
   * The event handler of the plugin status.
   *
   * @param {Event} messageEvent the event object from the plugin.
   * @private
   */
  PluginManager.prototype.onMessage_ = function(messageEvent) {
    if (this.state == PluginState.LOADED) {
      if (messageEvent.data == 'stopped')
        this.state = PluginState.SAMPLING_RATE_READY;
      return;
    }

    if (messageEvent.data == 'audio') {
      if (this.state < PluginState.READY)
        this.onReady_();
      this.state = PluginState.RECOGNIZING;
    } else if (messageEvent.data == 'stopped') {
      this.state = PluginState.READY;
    } else {
      var matched = recognitionPattern.exec(messageEvent.data);
      if (matched && matched[1] == 'hotword_ok_google')
        this.onRecognized_(Number(matched[2]));
    }
  };

  /**
   * The event handler when the plugin is loaded.
   *
   * @private
   */
  PluginManager.prototype.onLoad_ = function() {
    if (this.state == PluginState.UNINITIALIZED) {
      this.state = PluginState.LOADED;
      if (this.samplingRate_ && this.config_)
        this.initialize_(this.samplingRate_, this.config_);
    }
  };

  /**
   * Sends the initialization messages to the plugin. This method is private.
   * The initialization will happen from onLoad_ or scheduleInitialize.
   *
   * @param {number} samplingRate the sampling rate the plugin accepts.
   * @param {string} config the url of the config file.
   * @private
   */
  PluginManager.prototype.initialize_ = function(samplingRate, config) {
    $('recognizer').postMessage(
        pluginCommands.SET_SAMPLING_RATE + samplingRate);
    $('recognizer').postMessage(pluginCommands.SET_CONFIG + config);
  };

  /**
   * Initializes the plugin with the specified parameter, or schedules the
   * initialization if the plugin is not ready.
   *
   * @param {number} samplingRate the sampling rate the plugin accepts.
   * @param {string} config the url of the config file.
   */
  PluginManager.prototype.scheduleInitialize = function(samplingRate, config) {
    if (this.state == PluginState.UNINITIALIZED) {
      this.samplingRate_ = samplingRate;
      this.config_ = config;
    } else {
      this.initialize_(samplingRate, config);
    }
  };

  /**
   * Asks the plugin to start recognizing the hotword.
   */
  PluginManager.prototype.startRecognizer = function() {
    $('recognizer').postMessage(pluginCommands.START_RECOGNIZING);
  };

  /**
   * Asks the plugin to stop recognizing the hotword.
   */
  PluginManager.prototype.stopRecognizer = function() {
    $('recognizer').postMessage(pluginCommands.STOP_RECOGNIZING);
  };

  /**
   * Sends the actual audio wave data.
   *
   * @param {ArrayBuffer} data The audio data to be recognized.
   */
  PluginManager.prototype.sendAudioData = function(data) {
    $('recognizer').postMessage(data);
  };

  return {
    PluginManager: PluginManager,
    PluginState: PluginState,
    isPluginAvailable: isPluginAvailable
  };
});
