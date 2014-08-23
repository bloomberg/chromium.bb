// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
'use strict';

/**
 * Class used to manage the state of the NaCl recognizer plugin. Handles all
 * control of the NaCl plugin, including creation, start, stop, trigger, and
 * shutdown.
 *
 * @constructor
 * @extends {cr.EventTarget}
 */
function NaClManager() {
  /**
   * Current state of this manager.
   * @private {hotword.NaClManager.ManagerState_}
   */
  this.recognizerState_ = ManagerState_.UNINITIALIZED;

  /**
   * The window.timeout ID associated with a pending message.
   * @private {?number}
   */
  this.naclTimeoutId_ = null;

  /**
   * The expected message that will cancel the current timeout.
   * @private {?string}
   */
  this.expectingMessage_ = null;

  /**
   * Whether the plugin will be started as soon as it stops.
   * @private {boolean}
   */
  this.restartOnStop_ = false;

  /**
   * NaCl plugin element on extension background page.
   * @private {?Nacl}
   */
  this.plugin_ = null;

  /**
   * URL containing hotword-model data file.
   * @private {string}
   */
  this.modelUrl_ = '';

  /**
   * Media stream containing an audio input track.
   * @private {?MediaStream}
   */
  this.stream_ = null;
};

/**
 * States this manager can be in. Since messages to/from the plugin are
 * asynchronous (and potentially queued), it's not possible to know what state
 * the plugin is in. However, track a state machine for NaClManager based on
 * what messages are sent/received.
 * @enum {number}
 * @private
 */
NaClManager.ManagerState_ = {
  UNINITIALIZED: 0,
  LOADING: 1,
  STOPPING: 2,
  STOPPED: 3,
  STARTING: 4,
  RUNNING: 5,
  ERROR: 6,
  SHUTDOWN: 7,
};
var ManagerState_ = NaClManager.ManagerState_;
var Error_ = hotword.constants.Error;

NaClManager.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Called when an error occurs. Dispatches an event.
 * @param {!hotword.constants.Error} error
 * @private
 */
NaClManager.prototype.handleError_ = function(error) {
  event = new Event(hotword.constants.Event.ERROR);
  event.data = error;
  this.dispatchEvent(event);
};

/**
 * @return {boolean} True if the recognizer is in a running state.
 */
NaClManager.prototype.isRunning = function() {
  return this.recognizerState_ == ManagerState_.RUNNING;
};

/**
 * Set a timeout. Only allow one timeout to exist at any given time.
 * @param {!function()} func
 * @param {number} timeout
 * @private
 */
NaClManager.prototype.setTimeout_ = function(func, timeout) {
  assert(!this.naclTimeoutId_);
  this.naclTimeoutId_ = window.setTimeout(
      function() {
        this.naclTimeoutId_ = null;
        func();
      }.bind(this), timeout);
};

/**
 * Clears the current timeout.
 * @private
 */
NaClManager.prototype.clearTimeout_ = function() {
  window.clearTimeout(this.naclTimeoutId_);
  this.naclTimeoutId_ = null;
};

/**
 * Starts a stopped or stopping hotword recognizer (NaCl plugin).
 */
NaClManager.prototype.startRecognizer = function() {
  if (this.recognizerState_ == ManagerState_.STOPPED) {
    this.recognizerState_ = ManagerState_.STARTING;
    this.sendDataToPlugin_(hotword.constants.NaClPlugin.RESTART);
    this.waitForMessage_(hotword.constants.TimeoutMs.LONG,
                         hotword.constants.NaClPlugin.READY_FOR_AUDIO);
  } else if (this.recognizerState_ == ManagerState_.STOPPING) {
    // Wait until the plugin is stopped before trying to start it.
    this.restartOnStop_ = true;
  } else {
    throw 'Attempting to start NaCl recogniser not in STOPPED or STOPPING ' +
        'state';
  }
};

/**
 * Stops the hotword recognizer.
 */
NaClManager.prototype.stopRecognizer = function() {
  this.sendDataToPlugin_(hotword.constants.NaClPlugin.STOP);
  this.recognizerState_ = ManagerState_.STOPPING;
  this.waitForMessage_(hotword.constants.TimeoutMs.NORMAL,
                       hotword.constants.NaClPlugin.STOPPED);
};

/**
 * Checks whether the file at the given path exists.
 * @param {!string} path Path to a file. Can be any valid URL.
 * @return {boolean} True if the patch exists.
 * @private
 */
NaClManager.prototype.fileExists_ = function(path) {
  var xhr = new XMLHttpRequest();
  xhr.open('HEAD', path, false);
  try {
    xhr.send();
  } catch (err) {
    return false;
  }
  if (xhr.readyState != xhr.DONE || xhr.status != 200) {
    return false;
  }
  return true;
};

/**
 * Creates and returns a list of possible languages to check for hotword
 * support.
 * @return {!Array.<string>} Array of languages.
 * @private
 */
NaClManager.prototype.getPossibleLanguages_ = function() {
  // Create array used to search first for language-country, if not found then
  // search for language, if not found then no language (empty string).
  // For example, search for 'en-us', then 'en', then ''.
  var langs = new Array();
  if (hotword.constants.UI_LANGUAGE) {
    // Chrome webstore doesn't support uppercase path: crbug.com/353407
    var language = hotword.constants.UI_LANGUAGE.toLowerCase();
    langs.push(language);  // Example: 'en-us'.
    // Remove country to add just the language to array.
    var hyphen = language.lastIndexOf('-');
    if (hyphen >= 0) {
      langs.push(language.substr(0, hyphen));  // Example: 'en'.
    }
  }
  langs.push('');
  return langs;
};

/**
 * Creates a NaCl plugin object and attaches it to the page.
 * @param {!string} src Location of the plugin.
 * @return {!Nacl} NaCl plugin DOM object.
 * @private
 */
NaClManager.prototype.createPlugin_ = function(src) {
  var plugin = document.createElement('embed');
  plugin.src = src;
  plugin.type = 'application/x-nacl';
  document.body.appendChild(plugin);
  return plugin;
};

/**
 * Initializes the NaCl manager.
 * @param {!string} naclArch Either 'arm', 'x86-32' or 'x86-64'.
 * @param {!MediaStream} stream A stream containing an audio source track.
 * @return {boolean} True if the successful.
 */
NaClManager.prototype.initialize = function(naclArch, stream) {
  assert(this.recognizerState_ == ManagerState_.UNINITIALIZED);
  var langs = this.getPossibleLanguages_();
  var i, j;
  // For country-lang variations. For example, when combined with path it will
  // attempt to find: '/x86-32_en-gb/', else '/x86-32_en/', else '/x86-32_/'.
  for (i = 0; i < langs.length; i++) {
    var folder = hotword.constants.SHARED_MODULE_ROOT + '/_platform_specific/' +
        naclArch + '_' + langs[i] + '/';
    var dataSrc = folder + hotword.constants.File.RECOGNIZER_CONFIG;
    var pluginSrc = hotword.constants.SHARED_MODULE_ROOT + '/hotword_' +
        langs[i] + '.nmf';
    var dataExists = this.fileExists_(dataSrc) && this.fileExists_(pluginSrc);
    if (!dataExists) {
      continue;
    }

    var plugin = this.createPlugin_(pluginSrc);
    this.plugin_ = /** @type {Nacl} */ (plugin);
    if (!this.plugin_ || !this.plugin_.postMessage) {
      document.body.removeChild(this.plugin_);
      this.recognizerState_ = ManagerState_.ERROR;
      return false;
    }
    this.modelUrl_ = chrome.extension.getURL(dataSrc);
    this.stream_ = stream;
    this.recognizerState_ = ManagerState_.LOADING;

    plugin.addEventListener('message',
                            this.handlePluginMessage_.bind(this),
                            false);

    plugin.addEventListener('crash',
                            this.handleError_.bind(this, Error_.NACL_CRASH),
                            false);
    return true;
  }
  this.recognizerState_ = ManagerState_.ERROR;
  return false;
};

/**
 * Shuts down the NaCl plugin and frees all resources.
 */
NaClManager.prototype.shutdown = function() {
  if (this.plugin_ != null) {
    document.body.removeChild(this.plugin_);
    this.plugin_ = null;
  }
  this.clearTimeout_();
  this.recognizerState_ = ManagerState_.SHUTDOWN;
  this.stream_ = null;
};

/**
 * Sends data to the NaCl plugin.
 * @param {!string} data Command to be sent to NaCl plugin.
 * @private
 */
NaClManager.prototype.sendDataToPlugin_ = function(data) {
  assert(this.recognizerState_ != ManagerState_.UNINITIALIZED);
  this.plugin_.postMessage(data);
};

/**
 * Waits, with a timeout, for a message to be received from the plugin. If the
 * message is not seen within the timeout, dispatch an 'error' event and go into
 * the ERROR state.
 * @param {number} timeout Timeout, in milliseconds, to wait for the message.
 * @param {!string} message Message to wait for.
 * @private
 */
NaClManager.prototype.waitForMessage_ = function(timeout, message) {
  assert(this.expectingMessage_ == null,
         'Already waiting for message ' + this.expectingMessage_);
  this.setTimeout_(
      function() {
        this.recognizerState_ = ManagerState_.ERROR;
        this.handleError_(Error_.TIMEOUT);
      }.bind(this), timeout);
  this.expectingMessage_ = message;
};

/**
 * Called when a message is received from the plugin. If we're waiting for that
 * message, cancel the pending timeout.
 * @param {string} message Message received.
 * @private
 */
NaClManager.prototype.receivedMessage_ = function(message) {
  if (message == this.expectingMessage_) {
    this.clearTimeout_();
    this.expectingMessage_ = null;
  }
};

/**
 * Handle a REQUEST_MODEL message from the plugin.
 * The plugin sends this message immediately after starting.
 * @private
 */
NaClManager.prototype.handleRequestModel_ = function() {
  if (this.recognizerState_ != ManagerState_.LOADING) {
    return;
  }
  this.sendDataToPlugin_(
      hotword.constants.NaClPlugin.MODEL_PREFIX + this.modelUrl_);
  this.waitForMessage_(hotword.constants.TimeoutMs.LONG,
                       hotword.constants.NaClPlugin.MODEL_LOADED);
};

/**
 * Handle a MODEL_LOADED message from the plugin.
 * The plugin sends this message after successfully loading the language model.
 * @private
 */
NaClManager.prototype.handleModelLoaded_ = function() {
  if (this.recognizerState_ != ManagerState_.LOADING) {
    return;
  }
  this.sendDataToPlugin_(this.stream_.getAudioTracks()[0]);
  this.waitForMessage_(hotword.constants.TimeoutMs.LONG,
                       hotword.constants.NaClPlugin.MS_CONFIGURED);
};

/**
 * Handle a MS_CONFIGURED message from the plugin.
 * The plugin sends this message after successfully configuring the audio input
 * stream.
 * @private
 */
NaClManager.prototype.handleMsConfigured_ = function() {
  if (this.recognizerState_ != ManagerState_.LOADING) {
    return;
  }
  this.recognizerState_ = ManagerState_.STOPPED;
  this.dispatchEvent(new Event(hotword.constants.Event.READY));
};

/**
 * Handle a READY_FOR_AUDIO message from the plugin.
 * The plugin sends this message after the recognizer is started and
 * successfully receives and processes audio data.
 * @private
 */
NaClManager.prototype.handleReadyForAudio_ = function() {
  if (this.recognizerState_ != ManagerState_.STARTING) {
    return;
  }
  this.recognizerState_ = ManagerState_.RUNNING;
};

/**
 * Handle a HOTWORD_DETECTED message from the plugin.
 * The plugin sends this message after detecting the hotword.
 * @private
 */
NaClManager.prototype.handleHotwordDetected_ = function() {
  if (this.recognizerState_ != ManagerState_.RUNNING) {
    return;
  }
  // We'll receive a STOPPED message very soon.
  this.recognizerState_ = ManagerState_.STOPPING;
  this.waitForMessage_(hotword.constants.TimeoutMs.NORMAL,
                       hotword.constants.NaClPlugin.STOPPED);
  this.dispatchEvent(new Event(hotword.constants.Event.TRIGGER));
};

/**
 * Handle a STOPPED message from the plugin.
 * This plugin sends this message after stopping the recognizer. This can happen
 * either in response to a stop request, or after the hotword is detected.
 * @private
 */
NaClManager.prototype.handleStopped_ = function() {
  this.recognizerState_ = ManagerState_.STOPPED;
  if (this.restartOnStop_) {
    this.restartOnStop_ = false;
    this.startRecognizer();
  }
};

/**
 * Handles a message from the NaCl plugin.
 * @param {!Event} msg Message from NaCl plugin.
 * @private
 */
NaClManager.prototype.handlePluginMessage_ = function(msg) {
  if (msg['data']) {
    this.receivedMessage_(msg['data']);
    switch (msg['data']) {
      case hotword.constants.NaClPlugin.REQUEST_MODEL:
        this.handleRequestModel_();
        break;
      case hotword.constants.NaClPlugin.MODEL_LOADED:
        this.handleModelLoaded_();
        break;
      case hotword.constants.NaClPlugin.MS_CONFIGURED:
        this.handleMsConfigured_();
        break;
      case hotword.constants.NaClPlugin.READY_FOR_AUDIO:
        this.handleReadyForAudio_();
        break;
      case hotword.constants.NaClPlugin.HOTWORD_DETECTED:
        this.handleHotwordDetected_();
        break;
      case hotword.constants.NaClPlugin.STOPPED:
        this.handleStopped_();
        break;
    }
  }
};

return {
  NaClManager: NaClManager
};

});
