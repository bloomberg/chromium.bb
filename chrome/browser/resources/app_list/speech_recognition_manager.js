// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The manager of speech recognition.
 */

cr.define('speech', function() {
  'use strict';

  /**
   * @constructor
   */
  function SpeechRecognitionManager(delegate) {
    this.isActive = true;
    this.delegate_ = delegate;

    this.recognizer_ = new window.webkitSpeechRecognition();
    this.recognizer_.continuous = true;
    this.recognizer_.interimResults = true;
    // TODO(mukai): should switch to the user's UI language.
    this.recognizer_.lang = 'en_US';

    this.recognizer_.onresult = this.onRecognizerResult_.bind(this);
    if (this.delegate_) {
      this.recognizer_.onstart =
          this.delegate_.onSpeechRecognitionStarted.bind(this.delegate_);
      this.recognizer_.onend =
          this.delegate_.onSpeechRecognitionEnded.bind(this.delegate_);
      this.recognizer_.onerror =
          this.delegate_.onSpeechRecognitionError.bind(this.delegate_);
    }
  }

  /**
   * Called when new speech recognition results arrive.
   *
   * @param {Event} speechEvent The event to contain the recognition results.
   * @private
   */
  SpeechRecognitionManager.prototype.onRecognizerResult_ = function(
      speechEvent) {
    // Do not collect interim result for now.
    var result = '';
    var isFinal = false;
    for (var i = 0; i < speechEvent.results.length; i++) {
      if (speechEvent.results[i].isFinal)
        isFinal = true;
      result += speechEvent.results[i][0].transcript;
    }
    if (this.delegate_)
      this.delegate_.onSpeechRecognized(result, isFinal);
  };

  /**
   * Starts the speech recognition through webkitSpeechRecognition.
   */
  SpeechRecognitionManager.prototype.start = function() {
    this.recognizer_.start();
  };

  /**
   * Stops the ongoing speech recognition.
   */
  SpeechRecognitionManager.prototype.stop = function() {
    this.recognizer_.abort();
  };

  return {
    SpeechRecognitionManager: SpeechRecognitionManager
  };
});
