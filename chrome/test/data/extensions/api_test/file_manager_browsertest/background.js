// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Extension ID of Files.app.
 * @type {string}
 * @const
 */
var FILE_MANAGER_EXTENSIONS_ID = 'hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * Calls a remote test util in Files.app's extension. See: test_util.js.
 *
 * @param {string} func Function name.
 * @param {?string} appId Target window's App ID or null for functions
 *     not requiring a window.
 * @param {Array.<*>} args Array of arguments.
 * @param {function(*)} callback Callback handling the function's result.
 */
function callRemoteTestUtil(func, appId, args, callback) {
  chrome.runtime.sendMessage(
      FILE_MANAGER_EXTENSIONS_ID, {
        func: func,
        appId: appId,
        args: args
      },
      callback);
}

/**
 * Executes a sequence of test steps.
 * @constructor
 */
function StepsRunner() {
  /**
   * List of steps.
   * @type {Array.<function>}
   * @private
   */
  this.steps_ = [];
}

/**
 * Creates a StepsRunner instance and runs the passed steps.
 */
StepsRunner.run = function(steps) {
  var stepsRunner = new StepsRunner();
  stepsRunner.run_(steps);
};

StepsRunner.prototype = {
  /**
   * @return {function} The next closure.
   */
  get next() {
    return this.steps_[0];
  }
};

/**
 * Runs a sequence of the added test steps.
 * @type {Array.<function>} List of the sequential steps.
 */
StepsRunner.prototype.run_ = function(steps) {
  this.steps_ = steps.slice(0);

  // An extra step which acts as an empty callback for optional asynchronous
  // calls in the last provided step.
  this.steps_.push(function() {});

  this.steps_ = this.steps_.map(function(f) {
    return chrome.test.callbackPass(function() {
      this.steps_.shift();
      f.apply(this, arguments);
    }.bind(this));
  }.bind(this));

  this.next();
};

/**
 * Adds the givin entries to the target volume(s).
 * @param {Array.<string>} volumeNames Names of target volumes.
 * @param {Array.<TestEntryInfo>} entries List of entries to be added.
 * @param {function(boolean)} callback Callback function to be passed the result
 *     of function. The argument is true on success.
 */
function addEntries(volumeNames, entries, callback) {
  if (volumeNames.length == 0) {
    callback(true);
    return;
  }
  chrome.test.sendMessage(JSON.stringify({
    name: 'addEntries',
    volume: volumeNames.shift(),
    entries: entries
  }), chrome.test.callbackPass(function(result) {
    if (result == "onEntryAdded")
      addEntries(volumeNames, entries, callback);
    else
      callback(false);
  }));
};

var steps = [
  // Check for the guest mode.
  function() {
    chrome.test.sendMessage(
        JSON.stringify({name: 'isInGuestMode'}), steps.shift());
  },
  // Obtain the test case name.
  function(result) {
    if (JSON.parse(result) != chrome.extension.inIncognitoContext)
      return;
    chrome.test.sendMessage(
        JSON.stringify({name: 'getTestName'}), steps.shift());
  },
  // Run the test case.
  function(testCaseName) {
    if (!testcase[testCaseName])
      return;
    chrome.test.runTests([testcase[testCaseName]]);
  }
];

steps.shift()();
