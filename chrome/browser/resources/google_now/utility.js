// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
  // TODO(vadimt): Send UMAs instead of showing alert.
  // TODO(vadimt): Make sure the execution doesn't continue after this call.
  if (!condition) {
    var errorText = 'ASSERT: ' + message;
    console.error(errorText);
    alert(errorText);
  }
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
    entry.task(finish);
  }

  /**
   * Checks if a new task can be added to the task queue.
   * @param {string} taskName Name of the new task.
   * @return {boolean} Whether the new task can be added.
   */
  function canQueue(taskName) {
    for (var i = 0; i < queue.length; ++i) {
      if (areConflicting(taskName, queue[i].name))
        return false;
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
    // TODO(vadimt): Pass UMA counters instead of step names.
    stepName = step;
  }

  chrome.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == CANNOT_UNLOAD_ALARM_NAME) {
      // Error if the event page wasn't unloaded after a reasonable timeout
      // since starting the last task.
      // TODO(vadimt): Uncomment the verify once this bug is fixed:
      // crbug.com/177563
      // verify(false, 'Event page didn\'t unload, queue = ' +
      // JSON.stringify(tasks) + ', step = ' + stepName + ' (ignore this verify
      // if devtools is attached).');
    }
  });

  chrome.runtime.onSuspend.addListener(function() {
    chrome.alarms.clear(CANNOT_UNLOAD_ALARM_NAME);
    verify(queue.length == 0 && stepName == null,
      'Incomplete task when unloading event page, queue = ' +
      JSON.stringify(queue) + ', step = ' + stepName);
  });

  return {
    add: add,
    debugSetStepName: debugSetStepName
  };
}
