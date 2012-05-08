// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome = chrome || {};

// TODO(akalin): Add mocking code for e.g. chrome.send() so that we
// can test this without rebuilding chrome.

/**
 * Organize sync event listeners and asynchronous requests.
 * This object is one of a kind; its constructor is not public.
 * @type {Object}
 */
chrome.sync = chrome.sync || {};
(function() {

// This Event class is a simplified version of the one from
// event_bindings.js.
function Event() {
  this.listeners_ = [];
}

Event.prototype.addListener = function(listener) {
  this.listeners_.push(listener);
};

Event.prototype.removeListener = function(listener) {
  var i = this.findListener_(listener);
  if (i == -1) {
    return;
  }
  this.listeners_.splice(i, 1);
};

Event.prototype.hasListener = function(listener) {
  return this.findListener_(listener) > -1;
};

Event.prototype.hasListeners = function(listener) {
  return this.listeners_.length > 0;
};

// Returns the index of the given listener, or -1 if not found.
Event.prototype.findListener_ = function(listener) {
  for (var i = 0; i < this.listeners_.length; i++) {
    if (this.listeners_[i] == listener) {
      return i;
    }
  }
  return -1;
};

// Fires the event.  Called by the actual event callback.  Any
// exceptions thrown by a listener are caught and logged.
Event.prototype.fire = function() {
  var args = Array.prototype.slice.call(arguments);
  for (var i = 0; i < this.listeners_.length; i++) {
    try {
      this.listeners_[i].apply(null, args);
    } catch (e) {
      if (e instanceof Error) {
        // Non-standard, but useful.
        console.error(e.stack);
      } else {
        console.error(e);
      }
    }
  }
};

chrome.sync.events = {
  'service': [
    'onServiceStateChanged'
  ],

  // See chrome/browser/sync/engine/syncapi.h for docs.
  'notifier': [
    'onNotificationStateChange',
    'onIncomingNotification'
  ],

  'manager': [
    'onChangesApplied',
    'onChangesComplete',
    'onSyncCycleCompleted',
    'onConnectionStatusChange',
    'onUpdatedToken',
    'onPassphraseRequired',
    'onPassphraseAccepted',
    'onInitializationComplete',
    'onStopSyncingPermanently',
    'onClearServerDataSucceeded',
    'onClearServerDataFailed',
    'onEncryptedTypesChanged',
    'onEncryptionComplete',
    'onActionableError',
  ],

  'transaction': [
    'onTransactionWrite',
  ]
};

for (var eventType in chrome.sync.events) {
  var events = chrome.sync.events[eventType];
  for (var i = 0; i < events.length; ++i) {
    var event = events[i];
    chrome.sync[event] = new Event();
  }
}

function makeSyncFunction(name) {
  var callbacks = [];

  // Calls the function, assuming the last argument is a callback to be
  // called with the return value.
  var fn = function() {
    var args = Array.prototype.slice.call(arguments);
    callbacks.push(args.pop());
    chrome.send(name, args);
  };

  // Handle a reply, assuming that messages are processed in FIFO order.
  // Called by SyncInternalsUI::HandleJsReply().
  fn.handleReply = function() {
    var args = Array.prototype.slice.call(arguments);
    // Remove the callback before we call it since the callback may
    // throw.
    var callback = callbacks.shift();
    callback.apply(null, args);
  };

  return fn;
}

var syncFunctions = [
  // Sync service functions.
  'getAboutInfo',

  // Notification functions.  See chrome/browser/sync/engine/syncapi.h
  // for docs.
  'getNotificationState',
  'getNotificationInfo',

  // Client server communication logging functions.
  'getClientServerTraffic',

  // Node lookup functions.  See chrome/browser/sync/engine/syncapi.h
  // for docs.
  'getRootNodeDetails',
  'getNodeSummariesById',
  'getNodeDetailsById',
  'getChildNodeIds',
  'getAllNodes',
];

for (var i = 0; i < syncFunctions.length; ++i) {
  var syncFunction = syncFunctions[i];
  chrome.sync[syncFunction] = makeSyncFunction(syncFunction);
}

/**
 * Returns an object which measures elapsed time.
 */
chrome.sync.makeTimer = function() {
  var start = new Date();

  return {
    /**
     * @return {number} The number of seconds since the timer was
     * created.
     */
    get elapsedSeconds() {
      return ((new Date()).getTime() - start.getTime()) / 1000.0;
    }
  };
};

})();
