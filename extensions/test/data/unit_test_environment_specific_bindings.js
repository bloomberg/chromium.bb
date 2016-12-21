// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sendRequestNatives = requireNative('sendRequest');

function registerHooks(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('notifyPass', function() {
    requireAsync('testNatives').then(function(natives) {
      natives.NotifyPass();
    });
  });

  apiFunctions.setHandleRequest('notifyFail', function(message) {
    requireAsync('testNatives').then(function(natives) {
      natives.NotifyFail(message);
    });
  });

  apiFunctions.setHandleRequest('log', function() {
    requireAsync('testNatives').then(function(natives) {
      natives.Log($Array.join(arguments, ' '));
    });
  });

}

function testDone(runNextTest) {
    // Use a promise here to allow previous test contexts to be eligible for
    // garbage collection.
    Promise.resolve().then(function() {
      runNextTest();
    });
}

function exportTests(tests, runTests, exports) {
  $Array.forEach(tests, function(test) {
    exports[test.name] = function() {
      runTests([test]);
      return true;
    };
  });
}

/**
 * A fake implementation of setTimeout and clearTimeout.
 * @constructor
 */
function TimeoutManager() {
  this.timeouts_ = {};
  this.nextTimeoutId_ = 0;
  this.currentTime = 0;
  this.autorunEnabled_ = false;
}

/**
 * Installs setTimeout and clearTimeout into the global object.
 */
TimeoutManager.prototype.installGlobals = function() {
  var global = sendRequestNatives.GetGlobal({});
  global.setTimeout = this.setTimeout_.bind(this);
  global.clearTimeout = this.clearTimeout_.bind(this);
};

/**
 * Starts auto-running of timeout callbacks. Until |numCallbacksToRun| callbacks
 * have run, any timeout callbacks set by calls to setTimeout (including before
 * the call to run) will cause the currentTime to be advanced to the time of
 * the timeout.
 */
TimeoutManager.prototype.run = function(numCallbacksToRun) {
  this.numCallbacksToRun_ = numCallbacksToRun;
  Promise.resolve().then(this.autoRun_.bind(this));
};

/**
 * Runs timeout callbacks with earliest timeout.
 * @private
 */
TimeoutManager.prototype.autoRun_ = function() {
  if (this.numCallbacksToRun_ <= 0 || $Object.keys(this.timeouts_).length == 0)
    return;

  // Bucket the timeouts by their timeout time.
  var timeoutsByTimeout = {};
  var timeoutIds = $Object.keys(this.timeouts_);
  for (var i = 0; i < timeoutIds.length; i++) {
    var timeout = this.timeouts_[timeoutIds[i]];
    var timeMs = timeout.timeMs;
    if (!timeoutsByTimeout[timeMs])
      timeoutsByTimeout[timeMs] = [];
    timeoutsByTimeout[timeMs].push(timeout);
  }
  this.currentTime =
      $Function.apply(Math.min, null, $Object.keys((timeoutsByTimeout)));
  // Run all timeouts in the earliest timeout bucket.
  var timeouts = timeoutsByTimeout[this.currentTime];
  for (var i = 0; i < timeouts.length; i++) {
    var currentTimeout = timeouts[i];
    if (!this.timeouts_[currentTimeout.id])
      continue;
    this.numCallbacksToRun_--;
    delete this.timeouts_[currentTimeout.id];
    try {
      currentTimeout.target();
    } catch (e) {
      console.log('error calling timeout target ' + e.stack);
    }
  }
  // Continue running any later callbacks.
  Promise.resolve().then(this.autoRun_.bind(this));
};

/**
 * A fake implementation of setTimeout. This does not support passing callback
 * arguments.
 * @private
 */
TimeoutManager.prototype.setTimeout_ = function(target, timeoutMs) {
  var timeoutId = this.nextTimeoutId_++;
  this.timeouts_[timeoutId] = {
    id: timeoutId,
    target: target,
    timeMs: timeoutMs + this.currentTime,
  };
  if (this.autorunEnabled_)
    Promise.resolve().then(this.autoRun_.bind(this));
  return timeoutId;
};

/**
 * A fake implementation of clearTimeout.
 * @private
 */
TimeoutManager.prototype.clearTimeout_ = function(timeoutId) {
  if (this.timeouts_[timeoutId])
    delete this.timeouts_[timeoutId];
};

exports.registerHooks = registerHooks;
exports.testDone = testDone;
exports.exportTests = exportTests;
exports.TimeoutManager = TimeoutManager;
