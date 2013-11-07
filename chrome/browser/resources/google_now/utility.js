// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Utility objects and functions for Google Now extension.
 * Most important entities here:
 * (1) 'wrapper' is a module used to add error handling and other services to
 *     callbacks for HTML and Chrome functions and Chrome event listeners.
 *     Chrome invokes extension code through event listeners. Once entered via
 *     an event listener, the extension may call a Chrome/HTML API method
 *     passing a callback (and so forth), and that callback must occur later,
 *     otherwise, we generate an error. Chrome may unload event pages waiting
 *     for an event. When the event fires, Chrome will reload the event page. We
 *     don't require event listeners to fire because they are generally not
 *     predictable (like a location change event).
 * (2) Task Manager (built with buildTaskManager() call) provides controlling
 *     mutually excluding chains of callbacks called tasks. Task Manager uses
 *     WrapperPlugins to add instrumentation code to 'wrapper' to determine
 *     when a task completes.
 */

// TODO(vadimt): Use server name in the manifest.

/**
 * Notification server URL.
 */
var NOTIFICATION_CARDS_URL = 'https://www.googleapis.com/chromenow/v1';

var DEBUG_MODE = localStorage['debug_mode'];

/**
 * Initializes for debug or release modes of operation.
 */
function initializeDebug() {
  if (DEBUG_MODE) {
    NOTIFICATION_CARDS_URL =
        localStorage['server_url'] || NOTIFICATION_CARDS_URL;
  }
}

initializeDebug();

/**
 * Location Card Storage.
 */
if (localStorage['locationCardsShown'] === undefined)
  localStorage['locationCardsShown'] = 0;

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
 * @param {string} method Request method.
 * @param {string} handlerName Server handler to send the request to.
 * @param {string=} contentType Value for the Content-type header.
 * @return {XMLHttpRequest} Server request.
 */
function buildServerRequest(method, handlerName, contentType) {
  var request = new XMLHttpRequest();

  request.responseType = 'text';
  request.open(method, NOTIFICATION_CARDS_URL + '/' + handlerName, true);
  if (contentType)
    request.setRequestHeader('Content-type', contentType);

  return request;
}

/**
 * Sends an error report to the server.
 * @param {Error} error Error to send.
 */
function sendErrorReport(error) {
  // Don't remove 'error.stack.replace' below!
  var filteredStack = error.canSendMessageToServer ?
      error.stack : error.stack.replace(/.*\n/, '(message removed)\n');
  var file;
  var line;
  var topFrameLineMatch = filteredStack.match(/\n    at .*\n/);
  var topFrame = topFrameLineMatch && topFrameLineMatch[0];
  if (topFrame) {
    // Examples of a frame:
    // 1. '\n    at someFunction (chrome-extension://
    //     pafkbggdmjlpgkdkcbjmhmfcdpncadgh/background.js:915:15)\n'
    // 2. '\n    at chrome-extension://pafkbggdmjlpgkdkcbjmhmfcdpncadgh/
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
    // [N-3] //pafkbggdmjlpgkdkcbjmhmfcdpncadgh/utility.js
    // [N-2] 308
    // [N-1] 19
    if (topFrameElements.length >= 3) {
      file = topFrameElements[topFrameElements.length - 3];
      line = topFrameElements[topFrameElements.length - 2];
    }
  }

  var errorText = error.name;
  if (error.canSendMessageToServer)
    errorText = errorText + ': ' + error.message;

  var errorObject = {
    message: errorText,
    file: file,
    line: line,
    trace: filteredStack
  };

  var request = buildServerRequest('POST', 'jserrors', 'application/json');
  request.onloadend = function(event) {
    console.log('sendErrorReport status: ' + request.status);
  };

  chrome.identity.getAuthToken({interactive: false}, function(token) {
    if (token) {
      request.setRequestHeader('Authorization', 'Bearer ' + token);
      request.send(JSON.stringify(errorObject));
    }
  });
}

// Limiting 1 error report per background page load.
var errorReported = false;

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

// Partial mirror of chrome.* for all instrumented functions.
var instrumented = {};

/**
 * Wrapper plugin. These plugins extend instrumentation added by
 * wrapper.wrapCallback by adding code that executes before and after the call
 * to the original callback provided by the extension.
 *
 * @typedef {{
 *   prologue: function (),
 *   epilogue: function ()
 * }}
 */
var WrapperPlugin;

/**
 * Wrapper for callbacks. Used to add error handling and other services to
 * callbacks for HTML and Chrome functions and events.
 */
var wrapper = (function() {
  /**
   * Factory for wrapper plugins. If specified, it's used to generate an
   * instance of WrapperPlugin each time we wrap a callback (which corresponds
   * to addListener call for Chrome events, and to every API call that specifies
   * a callback). WrapperPlugin's lifetime ends when the callback for which it
   * was generated, exits. It's possible to have several instances of
   * WrapperPlugin at the same time.
   * An instance of WrapperPlugin can have state that can be shared by its
   * constructor, prologue() and epilogue(). Also WrapperPlugins can change
   * state of other objects, for example, to do refcounting.
   * @type {function(): WrapperPlugin}
   */
  var wrapperPluginFactory = null;

  /**
   * Registers a wrapper plugin factory.
   * @param {function(): WrapperPlugin} factory Wrapper plugin factory.
   */
  function registerWrapperPluginFactory(factory) {
    if (wrapperPluginFactory) {
      reportError(buildErrorWithMessageForServer(
          'registerWrapperPluginFactory: factory is already registered.'));
    }

    wrapperPluginFactory = factory;
  }

  /**
   * True if currently executed code runs in a callback or event handler that
   * was instrumented by wrapper.wrapCallback() call.
   * @type {boolean}
   */
  var isInWrappedCallback = false;

  /**
   * Required callbacks that are not yet called. Includes both task and non-task
   * callbacks. This is a map from unique callback id to the stack at the moment
   * when the callback was wrapped. This stack identifies the callback.
   * Used only for diagnostics.
   * @type {Object.<number, string>}
   */
  var pendingCallbacks = {};

  /**
   * Unique ID of the next callback.
   * @type {number}
   */
  var nextCallbackId = 0;

  /**
   * Gets diagnostic string with the status of the wrapper.
   * @return {string} Diagnostic string.
   */
  function debugGetStateString() {
    return 'pendingCallbacks = ' + JSON.stringify(pendingCallbacks);
  }

  /**
   * Checks that we run in a wrapped callback.
   */
  function checkInWrappedCallback() {
    if (!isInWrappedCallback) {
      reportError(buildErrorWithMessageForServer(
          'Not in instrumented callback'));
    }
  }

  /**
   * Adds error processing to an API callback.
   * @param {Function} callback Callback to instrument.
   * @param {boolean=} opt_isEventListener True if the callback is a listener to
   *     a Chrome API event.
   * @return {Function} Instrumented callback.
   */
  function wrapCallback(callback, opt_isEventListener) {
    var callbackId = nextCallbackId++;

    if (!opt_isEventListener) {
      checkInWrappedCallback();
      pendingCallbacks[callbackId] = new Error().stack;
    }

    // wrapperPluginFactory may be null before task manager is built, and in
    // tests.
    var wrapperPluginInstance = wrapperPluginFactory && wrapperPluginFactory();

    return function() {
      // This is the wrapper for the callback.
      try {
        verify(!isInWrappedCallback, 'Re-entering instrumented callback');
        isInWrappedCallback = true;

        if (!opt_isEventListener)
          delete pendingCallbacks[callbackId];

        if (wrapperPluginInstance)
          wrapperPluginInstance.prologue();

        // Call the original callback.
        callback.apply(null, arguments);

        if (wrapperPluginInstance)
          wrapperPluginInstance.epilogue();

        verify(isInWrappedCallback,
               'Instrumented callback is not instrumented upon exit');
        isInWrappedCallback = false;
      } catch (error) {
        reportError(error);
      }
    };
  }

  /**
   * Returns an instrumented function.
   * @param {!Array.<string>} functionIdentifierParts Path to the chrome.*
   *     function.
   * @param {string} functionName Name of the chrome API function.
   * @param {number} callbackParameter Index of the callback parameter to this
   *     API function.
   * @return {Function} An instrumented function.
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
      functionIdentifierParts.forEach(function(fragment) {
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
    functionIdentifierParts.forEach(function(fragment) {
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

  instrumentChromeApiFunction('runtime.onSuspend.addListener', 0);

  instrumented.runtime.onSuspend.addListener(function() {
    var stringifiedPendingCallbacks = JSON.stringify(pendingCallbacks);
    verify(
        stringifiedPendingCallbacks == '{}',
        'Pending callbacks when unloading event page:' +
        stringifiedPendingCallbacks);
  });

  return {
    wrapCallback: wrapCallback,
    instrumentChromeApiFunction: instrumentChromeApiFunction,
    registerWrapperPluginFactory: registerWrapperPluginFactory,
    checkInWrappedCallback: checkInWrappedCallback,
    debugGetStateString: debugGetStateString
  };
})();

wrapper.instrumentChromeApiFunction('alarms.get', 1);
wrapper.instrumentChromeApiFunction('alarms.onAlarm.addListener', 0);
wrapper.instrumentChromeApiFunction('identity.getAuthToken', 1);
wrapper.instrumentChromeApiFunction('identity.onSignInChanged.addListener', 0);
wrapper.instrumentChromeApiFunction('identity.removeCachedAuthToken', 1);
wrapper.instrumentChromeApiFunction('webstorePrivate.getBrowserLogin', 0);

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
   * @type {Array.<Object.<string, function()>>}
   */
  var queue = [];

  /**
   * Count of unfinished callbacks of the current task.
   * @type {number}
   */
  var taskPendingCallbackCount = 0;

  /**
   * True if currently executed code is a part of a task.
   * @type {boolean}
   */
  var isInTask = false;

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
        ', queue = ' + JSON.stringify(queue) + ', ' +
        wrapper.debugGetStateString());
    var entry = queue[0];
    console.log('Starting task ' + entry.name);

    entry.task();

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
   * @param {function()} task Function to run.
   */
  function add(taskName, task) {
    wrapper.checkInWrappedCallback();
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

  instrumented.runtime.onSuspend.addListener(function() {
    verify(
        queue.length == 0,
        'Incomplete task when unloading event page,' +
        ' queue = ' + JSON.stringify(queue) + ', ' +
        wrapper.debugGetStateString());
  });


  /**
   * Wrapper plugin for tasks.
   * @constructor
   */
  function TasksWrapperPlugin() {
    this.isTaskCallback = isInTask;
    if (this.isTaskCallback)
      ++taskPendingCallbackCount;
  }

  TasksWrapperPlugin.prototype = {
    /**
     * Plugin code to be executed before invoking the original callback.
     */
    prologue: function() {
      if (this.isTaskCallback) {
        verify(!isInTask, 'TasksWrapperPlugin.prologue: already in task');
        isInTask = true;
      }
    },

    /**
     * Plugin code to be executed after invoking the original callback.
     */
    epilogue: function() {
      if (this.isTaskCallback) {
        verify(isInTask, 'TasksWrapperPlugin.epilogue: not in task at exit');
        isInTask = false;
        if (--taskPendingCallbackCount == 0)
          finish();
      }
    }
  };

  wrapper.registerWrapperPluginFactory(function() {
    return new TasksWrapperPlugin();
  });

  return {
    add: add
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

// TODO(robliao): Use signed-in state change watch API when it's available.
/**
 * Wraps chrome.identity to provide limited listening support for
 * the sign in state by polling periodically for the auth token.
 * @return {Object} The Authentication Manager interface.
 */
function buildAuthenticationManager() {
  var alarmName = 'sign-in-alarm';

  /**
   * Gets an OAuth2 access token.
   * @param {function(string=)} callback Called on completion.
   *     The string contains the token. It's undefined if there was an error.
   */
  function getAuthToken(callback) {
    instrumented.identity.getAuthToken({interactive: false}, function(token) {
      token = chrome.runtime.lastError ? undefined : token;
      callback(token);
    });
  }

  /**
   * Determines whether there is an account attached to the profile.
   * @param {function(boolean)} callback Called on completion.
   */
  function isSignedIn(callback) {
    instrumented.webstorePrivate.getBrowserLogin(function(accountInfo) {
      callback(!!accountInfo.login);
    });
  }

  /**
   * Removes the specified cached token.
   * @param {string} token Authentication Token to remove from the cache.
   * @param {function} callback Called on completion.
   */
  function removeToken(token, callback) {
    instrumented.identity.removeCachedAuthToken({token: token}, function() {
      // Let Chrome now about a possible problem with the token.
      getAuthToken(function() {});
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

  /**
   * Checks if the last signed in state matches the current one.
   * If it doesn't, it notifies the listeners of the change.
   */
  function checkAndNotifyListeners() {
    isSignedIn(function(signedIn) {
      instrumented.storage.local.get('lastSignedInState', function(items) {
        items = items || {};
        if (items.lastSignedInState != signedIn) {
          chrome.storage.local.set(
              {lastSignedInState: signedIn});
          listeners.forEach(function(callback) {
            callback();
          });
        }
      });
    });
  }

  instrumented.identity.onSignInChanged.addListener(function() {
    checkAndNotifyListeners();
  });

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == alarmName)
      checkAndNotifyListeners();
  });

  // Poll for the sign in state every hour.
  // One hour is just an arbitrary amount of time chosen.
  chrome.alarms.create(alarmName, {periodInMinutes: 60});

  return {
    addListener: addListener,
    getAuthToken: getAuthToken,
    isSignedIn: isSignedIn,
    removeToken: removeToken
  };
}
