// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock tts class.
 * @constructor
 * @extends {cvox.TtsInterface}
 */
var MockTts = function() {
};

MockTts.prototype = {
  /**
   * A list of predicate, start, and end callbacks for a pending expectation.
   * @type {!Array<{{predicate: function(string) : boolean,
   *     startCallback: function() : void,
   *     endCallback: function() : void}>}
   * @private
   */
  expectations_: [],

  /**
   * A list of strings stored whenever there are no expectations.
   * @type {!Array<string}
   * @private
   */
  idleUtterances_: [],

  /** @override */
  speak: function(textString, queueMode, properties) {
    this.process_(textString, false, properties);
  },

  /**
   * Adds an expectation for the given string to be spoken. If satisfied,
   * |opt_callback| is called.
   * @param {string} expectedText
   * @param {function() : void=} opt_callback
   * @param {boolean=} opt_exact Expect an exact match; defaults to false.
   */
  expectSpeech: function(expectedText, opt_callback, opt_exact) {
    var expectation = {};
    expectation.endCallback = opt_callback;
    this.addExpectation_(expectedText, expectation, opt_exact);
  },

  /**
   * Adds an expectation for the given string to be spoken after |opt_callback|
   * executes.
   * @param {string} expectedText
   * @param {function() : void=} opt_callback
   * @param {boolean=} opt_exact Expect an exact match; defaults to false.
   */
  expectSpeechAfter: function(expectedText, opt_callback, opt_exact) {
    var expectation = {};
    if (this.expectations_.length == 0 && opt_callback)
      opt_callback();
    else
      expectation.startCallback = opt_callback;
    this.addExpectation_(expectedText, expectation, opt_exact);
  },

  /**
   * Finishes expectations and calls testDone.
   */
  finishExpectations: function() {
    this.expectSpeechAfter('', testDone);
  },

  /**
   * @private
   * @param {string} expectedText Text expected spoken.
   * @param {{startCallback: function() : void,
   *     endCallback: function() : void}=} opt_expectation
   * @param {boolean=} opt_exact Expects an exact match.
   */
  addExpectation_: function(expectedText, opt_expectation, opt_exact) {
    var expectation = opt_expectation ? opt_expectation : {};

    expectation.predicate = function(actualText) {
      if (opt_exact)
        return actualText === expectedText;
      return actualText.indexOf(expectedText) != -1;
    };

    this.expectations_.push(expectation);

    // Process any idleUtterances.
    this.idleUtterances_.forEach(function(utterance) {
      this.process_(utterance.text, true, utterance.properties);
    }.bind(this));
  },

  /**
   * @param {string} textString Utterance to match against callbacks.
   * @param {boolean=} opt_manual True if called outside of tts.speak.
   * @param {!Object=} opt_properties
   * @private
   */
  process_: function(textString, opt_manual, opt_properties) {
    var utterance = {text: textString, properties: opt_properties};
    if (this.expectations_.length == 0) {
      if (!opt_manual) {
        this.idleUtterances_.push(utterance);
      }
      return;
    }

    var allUtterances = this.idleUtterances_.concat([utterance]);
    var targetExpectation = this.expectations_.shift();
    allUtterances = allUtterances.filter(function(u) {
      return targetExpectation.predicate(u.text);
    });
    if (allUtterances.length > 0) {
      var matchingProperties = allUtterances[0].properties;
      this.idleUtterances_.length = 0;
      if (targetExpectation.endCallback)
        targetExpectation.endCallback();
      if (matchingProperties && matchingProperties.endCallback) {
        matchingProperties.endCallback();
      }
      var nextExpectation = this.expectations_[0];
      if (nextExpectation && nextExpectation.startCallback)
        nextExpectation.startCallback();
    } else {
      this.expectations_.unshift(targetExpectation);
    }
  },
};
