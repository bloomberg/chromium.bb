// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Utility objects and functions for Google Now extension.
 */

// TODO(vadimt): Figure out the server name. Use it in the manifest and for
// NOTIFICATION_CARDS_URL. Meanwhile, to use the feature, you need to manually
// set the server name via local storage.

/**
 * Notification server URL.
 */
var NOTIFICATION_CARDS_URL = localStorage['server_url'];

var DEBUG_MODE = localStorage['debug_mode'];

/**
 * Builds an error object with a message that may be sent to the server.
 * @param {string} message Error message. This message may be sent to the
 *     server.
 * @return {Error} Error object.
 */
function buildErrorWithMessageForServer(message) {
  var error = new Error(message);
  error.canSendMessageToServer = true;
  return error;
}

/**
 * Checks for internal errors.
 * @param {boolean} condition Condition that must be true.
 * @param {string} message Diagnostic message for the case when the condition is
 *     false.
 */
function verify(condition, message) {
  if (!condition)
    throw buildErrorWithMessageForServer('ASSERT: ' + message);
}

/**
 * Builds a request to the notification server.
 * @param {string} handlerName Server handler to send the request to.
 * @param {string} contentType Value for the Content-type header.
 * @return {XMLHttpRequest} Server request.
 */
function buildServerRequest(handlerName, contentType) {
  var request = new XMLHttpRequest();

  request.responseType = 'text';
  request.open('POST', NOTIFICATION_CARDS_URL + '/' + handlerName, true);
  request.setRequestHeader('Content-type', contentType);

  return request;
}

// Partial mirror of chrome.* for all instrumented functions.
var instrumented = {};

/**
 * Builds the object to manage tasks (mutually exclusive chains of events).
 * @param {function(string, string): boolean} areConflicting Function that
 *     checks if a new task can't be added to a task queue that contains an
 *     existing task.
 * @return {Object} Task manager interface.
 */
function buildTaskManager(areConflicting) {
  /**
   * Queue of scheduled tasks. The first element, if present, corresponds to the
   * currently running task.
   * @type {Array.<Object.<string, function(function())>>}
   */
  var queue = [];

  /**
   * Count of unfinished callbacks of the current task.
   * @type {number}
   */
  var taskPendingCallbackCount = 0;

  /**
   * Required callbacks that are not yet called. Includes both task and non-task
   * callbacks. This is a map from unique callback id to the stack at the moment
   * when the callback was wrapped. This stack identifies the callback.
   * Used only for diagnostics.
   * @type {Object.<number, string>}
   */
  var pendingCallbacks = {};

  /**
   * True if currently executed code is a part of a task.
   * @type {boolean}
   */
  var isInTask = false;

  /**
   * True if currently executed code runs in an instrumented callback.
   * @type {boolean}
   */
  var isInInstrumentedCallback = false;

  /**
   * Checks that we run in an instrumented callback.
   */
  function checkInInstrumentedCallback() {
    if (!isInInstrumentedCallback) {
      reportError(buildErrorWithMessageForServer(
          'Not in instrumented callback'));
    }
  }

  /**
   * Starts the first queued task.
   */
  function startFirst() {
    verify(queue.length >= 1, 'startFirst: queue is empty');
    verify(!isInTask, 'startFirst: already in task');
    isInTask = true;

    // Start the oldest queued task, but don't remove it from the queue.
    verify(
        taskPendingCallbackCount == 0,
        'tasks.startFirst: still have pending task callbacks: ' +
        taskPendingCallbackCount +
        ', queue = ' + JSON.stringify(queue) +
        ', pendingCallbacks = ' + JSON.stringify(pendingCallbacks));
    var entry = queue[0];
    console.log('Starting task ' + entry.name);

    entry.task(function() {});  // TODO(vadimt): Don't pass parameter.

    verify(isInTask, 'startFirst: not in task at exit');
    isInTask = false;
    if (taskPendingCallbackCount == 0)
      finish();
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
   *     parameter. Call this callback on completion.
   */
  function add(taskName, task) {
    checkInInstrumentedCallback();
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
           'tasks.finish: The task queue is empty');
    console.log('Finishing task ' + queue[0].name);
    queue.shift();

    if (queue.length >= 1)
      startFirst();
  }

  // Limiting 1 error report per background page load.
  var errorReported = false;

  /**
   * Sends an error report to the server.
   * @param {Error} error Error to send.
   */
  function sendErrorReport(error) {
    var filteredStack = error.canSendMessageToServer ?
        error.stack : error.stack.replace(/.*\n/, '(message removed)\n');
    var file;
    var line;
    var topFrameLineMatch = filteredStack.match(/\n    at .*\n/);
    var topFrame = topFrameLineMatch && topFrameLineMatch[0];
    if (topFrame) {
      // Examples of a frame:
      // 1. '\n    at someFunction (chrome-extension://
      //     pmofbkohncoogjjhahejjfbppikbjigm/background.js:915:15)\n'
      // 2. '\n    at chrome-extension://pmofbkohncoogjjhahejjfbppikbjigm/
      //     utility.js:269:18\n'
      // 3. '\n    at Function.target.(anonymous function) (extensions::
      //     SafeBuiltins:19:14)\n'
      // 4. '\n    at Event.dispatchToListener (event_bindings:382:22)\n'
      var errorLocation;
      // Find the the parentheses at the end of the line, if any.
      var parenthesesMatch = topFrame.match(/\(.*\)\n/);
      if (parenthesesMatch && parenthesesMatch[0]) {
        errorLocation =
            parenthesesMatch[0].substring(1, parenthesesMatch[0].length - 2);
      } else {
        errorLocation = topFrame;
      }

      var topFrameElements = errorLocation.split(':');
      // topFrameElements is an array that ends like:
      // [N-3] //pmofbkohncoogjjhahejjfbppikbjigm/utility.js
      // [N-2] 308
      // [N-1] 19
      if (topFrameElements.length >= 3) {
        file = topFrameElements[topFrameElements.length - 3];
        line = topFrameElements[topFrameElements.length - 2];
      }
    }

    var requestParameters =
        'error=' + encodeURIComponent(error.name) +
        '&script=' + encodeURIComponent(file) +
        '&line=' + encodeURIComponent(line) +
        '&trace=' + encodeURIComponent(filteredStack);
    var request = buildServerRequest('jserror',
                                     'application/x-www-form-urlencoded');
    request.onloadend = function(event) {
      console.log('sendErrorReport status: ' + request.status);
    };
    request.send(requestParameters);
  }

  /**
   * Reports an error to the server and the user, as appropriate.
   * @param {Error} error Error to report.
   */
  function reportError(error) {
    var message = 'Critical error:\n' + error.stack;
    console.error(message);
    if (!errorReported) {
      errorReported = true;
      chrome.metricsPrivate.getIsCrashReportingEnabled(function(isEnabled) {
        if (isEnabled)
          sendErrorReport(error);
        if (DEBUG_MODE)
          alert(message);
      });
    }
  }

  /**
   * Unique ID of the next callback.
   * @type {number}
   */
  var nextCallbackId = 0;

  /**
   * Adds error processing to an API callback.
   * @param {Function} callback Callback to instrument.
   * @param {boolean=} opt_isEventListener True if the callback is an event
   *      listener.
   * @return {Function} Instrumented callback.
   */
  function wrapCallback(callback, opt_isEventListener) {
    verify(!(opt_isEventListener && isInTask),
           'Unrequired callback in a task.');
    var callbackId = nextCallbackId++;
    var isTaskCallback = isInTask;
    if (isTaskCallback)
      ++taskPendingCallbackCount;
    if (!opt_isEventListener) {
      checkInInstrumentedCallback();
      pendingCallbacks[callbackId] = new Error().stack;
    }

    return function() {
      // This is the wrapper for the callback.
      try {
        verify(!isInInstrumentedCallback, 'Re-entering instrumented callback');
        isInInstrumentedCallback = true;

        if (isTaskCallback) {
          verify(!isInTask, 'wrapCallback: already in task');
          isInTask = true;
        }
        if (!opt_isEventListener)
          delete pendingCallbacks[callbackId];

        // Call the original callback.
        callback.apply(null, arguments);

        if (isTaskCallback) {
          verify(isInTask, 'wrapCallback: not in task at exit');
          isInTask = false;
          if (--taskPendingCallbackCount == 0)
            finish();
        }

        verify(isInInstrumentedCallback,
               'Instrumented callback is not instrumented upon exit');
        isInInstrumentedCallback = false;
      } catch (error) {
        reportError(error);
      }
    };
  }

  /**
   * Returns an instrumented function.
   * @param {array} functionIdentifierParts Path to the chrome.* function.
   * @param {string} functionName Name of the chrome API function.
   * @param {number} callbackParameter Index of the callback parameter to this
   *     API function.
   * @return {function} An instrumented function.
   */
  function createInstrumentedFunction(
      functionIdentifierParts,
      functionName,
      callbackParameter) {
    return function() {
      // This is the wrapper for the API function. Pass the wrapped callback to
      // the original function.
      var callback = arguments[callbackParameter];
      if (typeof callback != 'function') {
        reportError(buildErrorWithMessageForServer(
            'Argument ' + callbackParameter + ' of ' +
            functionIdentifierParts.join('.') + '.' + functionName +
            ' is not a function'));
      }
      arguments[callbackParameter] = wrapCallback(
          callback, functionName == 'addListener');

      var chromeContainer = chrome;
      functionIdentifierParts.map(function(fragment) {
        chromeContainer = chromeContainer[fragment];
      });
      return chromeContainer[functionName].
          apply(chromeContainer, arguments);
    };
  }

  /**
   * Instruments an API function to add error processing to its user
   * code-provided callback.
   * @param {string} functionIdentifier Full identifier of the function without
   *     the 'chrome.' portion.
   * @param {number} callbackParameter Index of the callback parameter to this
   *     API function.
   */
  function instrumentChromeApiFunction(functionIdentifier, callbackParameter) {
    var functionIdentifierParts = functionIdentifier.split('.');
    var functionName = functionIdentifierParts.pop();
    var chromeContainer = chrome;
    var instrumentedContainer = instrumented;
    functionIdentifierParts.map(function(fragment) {
      chromeContainer = chromeContainer[fragment];
      if (!chromeContainer) {
        reportError(buildErrorWithMessageForServer(
            'Cannot instrument ' + functionIdentifier));
      }

      if (!(fragment in instrumentedContainer))
        instrumentedContainer[fragment] = {};

      instrumentedContainer = instrumentedContainer[fragment];
    });

    var targetFunction = chromeContainer[functionName];
    if (!targetFunction) {
      reportError(buildErrorWithMessageForServer(
          'Cannot instrument ' + functionIdentifier));
    }

    instrumentedContainer[functionName] = createInstrumentedFunction(
        functionIdentifierParts,
        functionName,
        callbackParameter);
  }

  instrumentChromeApiFunction('alarms.get', 1);
  instrumentChromeApiFunction('alarms.onAlarm.addListener', 0);
  instrumentChromeApiFunction('identity.getAuthToken', 1);
  instrumentChromeApiFunction('identity.removeCachedAuthToken', 1);
  instrumentChromeApiFunction('runtime.onSuspend.addListener', 0);

  chrome.runtime.onSuspend.addListener(function() {
    var stringifiedPendingCallbacks = JSON.stringify(pendingCallbacks);
    verify(
        queue.length == 0 && stringifiedPendingCallbacks == '{}',
        'Incomplete task or pending callbacks when unloading event page,' +
        ' queue = ' + JSON.stringify(queue) +
        ', pendingCallbacks = ' + stringifiedPendingCallbacks);
  });

  return {
    add: add,
    debugSetStepName: function() {},  // TODO(vadimt): remove
    instrumentChromeApiFunction: instrumentChromeApiFunction,
    wrapCallback: wrapCallback
  };
}

/**
 * Builds an object to manage retrying activities with exponential backoff.
 * @param {string} name Name of this attempt manager.
 * @param {function()} attempt Activity that the manager retries until it
 *     calls 'stop' method.
 * @param {number} initialDelaySeconds Default first delay until first retry.
 * @param {number} maximumDelaySeconds Maximum delay between retries.
 * @return {Object} Attempt manager interface.
 */
function buildAttemptManager(
    name, attempt, initialDelaySeconds, maximumDelaySeconds) {
  var alarmName = 'attempt-scheduler-' + name;
  var currentDelayStorageKey = 'current-delay-' + name;

  /**
   * Creates an alarm for the next attempt. The alarm is repeating for the case
   * when the next attempt crashes before registering next alarm.
   * @param {number} delaySeconds Delay until next retry.
   */
  function createAlarm(delaySeconds) {
    var alarmInfo = {
      delayInMinutes: delaySeconds / 60,
      periodInMinutes: maximumDelaySeconds / 60
    };
    chrome.alarms.create(alarmName, alarmInfo);
  }

  /**
   * Indicates if this attempt manager has started.
   * @param {function(boolean)} callback The function's boolean parameter is
   *     true if the attempt manager has started, false otherwise.
   */
  function isRunning(callback) {
    instrumented.alarms.get(alarmName, function(alarmInfo) {
      callback(!!alarmInfo);
    });
  }

  /**
   * Schedules next attempt.
   * @param {number=} opt_previousDelaySeconds Previous delay in a sequence of
   *     retry attempts, if specified. Not specified for scheduling first retry
   *     in the exponential sequence.
   */
  function scheduleNextAttempt(opt_previousDelaySeconds) {
    var base = opt_previousDelaySeconds ? opt_previousDelaySeconds * 2 :
                                          initialDelaySeconds;
    var newRetryDelaySeconds =
        Math.min(base * (1 + 0.2 * Math.random()), maximumDelaySeconds);

    createAlarm(newRetryDelaySeconds);

    var items = {};
    items[currentDelayStorageKey] = newRetryDelaySeconds;
    chrome.storage.local.set(items);
  }

  /**
   * Starts repeated attempts.
   * @param {number=} opt_firstDelaySeconds Time until the first attempt, if
   *     specified. Otherwise, initialDelaySeconds will be used for the first
   *     attempt.
   */
  function start(opt_firstDelaySeconds) {
    if (opt_firstDelaySeconds) {
      createAlarm(opt_firstDelaySeconds);
      chrome.storage.local.remove(currentDelayStorageKey);
    } else {
      scheduleNextAttempt();
    }
  }

  /**
   * Stops repeated attempts.
   */
  function stop() {
    chrome.alarms.clear(alarmName);
    chrome.storage.local.remove(currentDelayStorageKey);
  }

  /**
   * Plans for the next attempt.
   * @param {function()} callback Completion callback. It will be invoked after
   *     the planning is done.
   */
  function planForNext(callback) {
    instrumented.storage.local.get(currentDelayStorageKey, function(items) {
      if (!items) {
        items = {};
        items[currentDelayStorageKey] = maximumDelaySeconds;
      }
      console.log('planForNext-get-storage ' + JSON.stringify(items));
      scheduleNextAttempt(items[currentDelayStorageKey]);
      callback();
    });
  }

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == alarmName)
      isRunning(function(running) {
        if (running)
          attempt();
      });
  });

  return {
    start: start,
    planForNext: planForNext,
    stop: stop,
    isRunning: isRunning
  };
}

// TODO(robliao): Ideally, the authentication watcher infrastructure
// below would be an API change to chrome.identity.
// When this happens, remove the code below.

/**
 * Wraps chrome.identity to provide limited listening support for
 * the sign in state by polling periodically for the auth token.
 * @return {Object} The Authentication Manager interface.
 */
function buildAuthenticationManager() {
  var alarmName = 'sign-in-alarm';

  /**
   * Determines if the user is signed in and provides a token if signed in.
   * @param {function(string=)} callback Called on completion.
   *     If the user is signed in, the string contains the token.
   */
  function isSignedIn(callback) {
    instrumented.identity.getAuthToken({interactive: false}, function(token) {
      token = chrome.runtime.lastError ? undefined : token;
      callback(token);
      checkAndNotifyListeners(!!token);
    });
  }

  /**
   * Removes the specified cached token.
   * @param {string} token Authentication Token to remove from the cache.
   * @param {function} callback Called on completion.
   */
  function removeToken(token, callback) {
    instrumented.identity.removeCachedAuthToken({token: token}, function() {
      // Removing the token from the cache will change the sign in state.
      // Repoll now to check the state and notify listeners.
      // This also lets Chrome now about a possible problem with the token.
      isSignedIn(function() {});
      callback();
    });
  }

  var listeners = [];

  /**
   * Registers a listener that gets called back when the signed in state
   * is found to be changed.
   * @param {function} callback Called when the answer to isSignedIn changes.
   */
  function addListener(callback) {
    listeners.push(callback);
  }

  // Tracks the last answer of isSignedIn. checkAndNotifyListeners will not
  // notify the listeners if this is null because technically, no sign in
  // state change occurred.
  var lastReturnedSignedInState = null;

  function checkAndNotifyListeners(currentSignedInState) {
    if ((lastReturnedSignedInState !== currentSignedInState) &&
        (lastReturnedSignedInState !== null)) {
      for (var listenerIndex in listeners) {
        listeners[listenerIndex]();
      }
    }
    lastReturnedSignedInState = currentSignedInState;
  }

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == alarmName)
      isSignedIn(function() {});
  });

  // Poll for the sign in state every hour.
  // One hour is just an arbitrary amount of time chosen.
  chrome.alarms.create(alarmName, {periodInMinutes: 60});

  return {
    addListener: addListener,
    isSignedIn: isSignedIn,
    removeToken: removeToken
  };
}
