// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 'use strict'; TODO(vadimt): Uncomment once crbug.com/237617 is fixed.

// TODO(vadimt): Remove alerts.

/**
 * @fileoverview Utility objects and functions for Google Now extension.
 */

/**
 * Checks for internal errors.
 * @param {boolean} condition Condition that must be true.
 * @param {string} message Diagnostic message for the case when the condition is
 *     false.
 */
function verify(condition, message) {
  if (!condition)
    throw new Error('ASSERT: ' + message);
}

/**
 * Builds the object to manage tasks (mutually exclusive chains of events).
 * @param {function(string, string): boolean} areConflicting Function that
 *     checks if a new task can't be added to a task queue that contains an
 *     existing task.
 * @return {Object} Task manager interface.
 */
function buildTaskManager(areConflicting) {
  /**
   * Name of the alarm that triggers the error saying that the event page cannot
   * unload.
   */
  var CANNOT_UNLOAD_ALARM_NAME = 'CANNOT-UNLOAD';

  /**
   * Maximal time we expect the event page to stay loaded after starting a task.
   */
  var MAXIMUM_LOADED_TIME_MINUTES = 5;

  /**
   * Queue of scheduled tasks. The first element, if present, corresponds to the
   * currently running task.
   * @type {Array.<Object.<string, function(function())>>}
   */
  var queue = [];

  /**
   * Name of the current step of the currently running task if present,
   * otherwise, null. For diagnostics only.
   * It's set when the task is started and before each asynchronous operation.
   */
  var stepName = null;

  /**
   * Starts the first queued task.
   */
  function startFirst() {
    verify(queue.length >= 1, 'startFirst: queue is empty');

    // Set alarm to verify that the event page will unload in a reasonable time.
    chrome.alarms.create(CANNOT_UNLOAD_ALARM_NAME,
                         {delayInMinutes: MAXIMUM_LOADED_TIME_MINUTES});

    // Start the oldest queued task, but don't remove it from the queue.
    verify(
        stepName == null,
        'tasks.startFirst: stepName is not null: ' + stepName +
        ', queue = ' + JSON.stringify(queue));
    var entry = queue[0];
    stepName = entry.name + '-initial';
    console.log('Starting task ' + entry.name);
    entry.task(finish);
  }

  /**
   * Checks if a new task can be added to the task queue.
   * @param {string} taskName Name of the new task.
   * @return {boolean} Whether the new task can be added.
   */
  function canQueue(taskName) {
    for (var i = 0; i < queue.length; ++i) {
      if (areConflicting(taskName, queue[i].name)) {
        console.log('Conflict: new=' + taskName +
                    ', scheduled=' + queue[i].name);
        return false;
      }
    }

    return true;
  }

  /**
   * Adds a new task. If another task is not running, runs the task immediately.
   * If any task in the queue is not compatible with the task, ignores the new
   * task. Otherwise, stores the task for future execution.
   * @param {string} taskName Name of the task.
   * @param {function(function())} task Function to run. Takes a callback
   *     parameter.
   */
  function add(taskName, task) {
    console.log('Adding task ' + taskName);
    if (!canQueue(taskName))
      return;

    queue.push({name: taskName, task: task});

    if (queue.length == 1) {
      startFirst();
    }
  }

  /**
   * Completes the current task and starts the next queued task if available.
   */
  function finish() {
    verify(queue.length >= 1,
           'tasks.finish: The task queue is empty; step = ' + stepName);
    console.log('Finishing task ' + queue[0].name);
    queue.shift();
    stepName = null;

    if (queue.length >= 1)
      startFirst();
  }

  /**
   * Associates a name with the current step of the task. Used for diagnostics
   * only. A task is a chain of asynchronous events; debugSetStepName should be
   * called before starting any asynchronous operation.
   * @param {string} step Name of new step.
   */
  function debugSetStepName(step) {
    stepName = step;
  }

  // Limiting 1 alert per background page load.
  var alertShown = false;

  /**
   * Adds error processing to an API callback.
   * @param {Function} callback Callback to instrument.
   * @return {Function} Instrumented callback.
   */
  function wrapCallback(callback) {
    return function() {
      // This is the wrapper for the callback.
      try {
        return callback.apply(null, arguments);
      } catch (error) {
        var message = 'Uncaught exception:\n' + error.stack;
        console.error(message);
        if (!alertShown) {
          alertShown = true;
          alert(message);
        }
      }
    };
  }

  /**
   * Instruments an API function to add error processing to its user
   * code-provided callback.
   * @param {Object} namespace Namespace of the API function.
   * @param {string} functionName Name of the API function.
   * @param {number} callbackParameter Index of the callback parameter to this
   *     API function.
   */
  function instrumentApiFunction(namespace, functionName, callbackParameter) {
    var originalFunction = namespace[functionName];

    if (!originalFunction)
      alert('Cannot instrument ' + functionName);

    namespace[functionName] = function() {
      // This is the wrapper for the API function. Pass the wrapped callback to
      // the original function.
      var callback = arguments[callbackParameter];
      if (typeof callback != 'function') {
        alert('Argument ' + callbackParameter + ' of ' + functionName +
              ' is not a function');
      }
      arguments[callbackParameter] = wrapCallback(callback);
      return originalFunction.apply(namespace, arguments);
    };
  }

  instrumentApiFunction(chrome.alarms.onAlarm, 'addListener', 0);
  instrumentApiFunction(chrome.runtime.onSuspend, 'addListener', 0);

  chrome.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == CANNOT_UNLOAD_ALARM_NAME) {
      // Error if the event page wasn't unloaded after a reasonable timeout
      // since starting the last task.
      verify(false, 'Event page didn\'t unload, queue=' +
          JSON.stringify(queue) + ', step=' + stepName +
          ' (ignore this assert if devtools is open).');
    }
  });

  chrome.runtime.onSuspend.addListener(function() {
    chrome.alarms.clear(CANNOT_UNLOAD_ALARM_NAME);

    verify(
        queue.length == 0,
        'Incomplete task when unloading event page, queue = ' +
        JSON.stringify(queue) + ', step = ' + stepName);
    verify(
        stepName == null,
        'Step name not null when unloading event page, queue = ' +
        JSON.stringify(queue) + ', step = ' + stepName);
  });

  return {
    add: add,
    // TODO(vadimt): Replace with instrumenting callbacks.
    debugSetStepName: debugSetStepName,
    instrumentApiFunction: instrumentApiFunction,
    wrapCallback: wrapCallback
  };
}
