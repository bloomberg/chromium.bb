// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mocks for globals needed for loading background.js.

function emptyMock() {}
function buildTaskManager() {
  return {instrumentApiFunction: emptyMock};
}
var instrumentApiFunction = emptyMock;
var buildAttemptManager = emptyMock;
var emptyListener = {addListener: emptyMock};
chrome['location'] = {onLocationUpdate: emptyListener};
chrome['notifications'] = {
  onButtonClicked: emptyListener,
  onClicked: emptyListener,
  onClosed: emptyListener
};
chrome['omnibox'] = {onInputEntered: emptyListener};
chrome['runtime'] = {
  onInstalled: emptyListener,
  onStartup: emptyListener
};

var storage = {};

/**
 * Syntactic sugar for use with will() on a Mock4JS.Mock.
 * Creates an action for will() that invokes a callback that the tested code
 * passes to a mocked function.
 * @param {SaveMockArguments} savedArgs Arguments that will contain the
 *     callback once the mocked function is called.
 * @param {number} callbackParameter Index of the callback parameter in
 *     |savedArgs|.
 * @param {...Object} var_args Arguments to pass to the callback.
 * @return {CallFunctionAction} Action for use in will().
 */
function invokeCallback(savedArgs, callbackParameter, var_args) {
  var callbackArguments = Array.prototype.slice.call(arguments, 2);
  return callFunction(function() {
    savedArgs.arguments[callbackParameter].apply(null, callbackArguments);
  });
}

/**
  * Mock4JS matcher object that matches the actual agrument and the expected
  * value iff their JSON represenations are same.
  * @param {Object} expectedValue Expected value.
  * @constructor
  */
function MatchJSON(expectedValue) {
  this.expectedValue_ = expectedValue;
}

MatchJSON.prototype = {
  /**
    * Checks that JSON represenation of the actual and expected arguments are
    * same.
    * @param {Object} actualArgument The argument to match.
    * @return {boolean} Result of the comparison.
    */
  argumentMatches: function(actualArgument) {
    return JSON.stringify(this.expectedValue_) ===
        JSON.stringify(actualArgument);
  },

  /**
    * Describes the matcher.
    * @return {string} Description of this Mock4JS matcher.
    */
  describe: function() {
    return 'eqJSON(' + JSON.stringify(this.expectedValue_) + ')';
  }
};

/**
  * Builds a MatchJSON agrument matcher for a given expected value.
  * @param {Object} expectedValue Expected value.
  * @return {MatchJSON} Resulting Mock4JS matcher.
  */
function eqJSON(expectedValue) {
  return new MatchJSON(expectedValue);
}
