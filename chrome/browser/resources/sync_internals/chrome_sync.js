// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require cr.js
// require cr/event_target.js
// require cr/util.js

cr.define('chrome.sync', function() {
  'use strict';

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
    // Client server communication logging functions.
    'getClientServerTraffic',

    // Get an array containing a JSON representations of all known sync nodes.
    'getAllNodes',
  ];

  for (var i = 0; i < syncFunctions.length; ++i) {
    var syncFunction = syncFunctions[i];
    chrome.sync[syncFunction] = makeSyncFunction(syncFunction);
  }

  /**
   * A simple timer to measure elapsed time.
   * @constructor
   */
  function Timer() {
    /**
     * The time that this Timer was created.
     * @type {number}
     * @private
     * @const
     */
    this.start_ = Date.now();
  }

  /**
   * @return {number} The elapsed seconds since this Timer was created.
   */
  Timer.prototype.getElapsedSeconds = function() {
    return (Date.now() - this.start_) / 1000;
  };

  /** @return {!Timer} An object which measures elapsed time. */
  var makeTimer = function() {
    return new Timer;
  };

  /**
   * @param {string} name The name of the event type.
   * @param {!Object} details A collection of event-specific details.
   */
  var dispatchEvent = function(name, details) {
    var e = new Event(name);
    e.details = details;
    chrome.sync.events.dispatchEvent(e);
  };

  /**
   * Asks the browser to refresh our snapshot of sync state.  Should result
   * in an onAboutInfoUpdated event being emitted.
   */
  var requestUpdatedAboutInfo = function() {
    chrome.send('requestUpdatedAboutInfo');
  };

  /**
   * Asks the browser to send us the list of registered types.  Should result
   * in an onReceivedListOfTypes event being emitted.
   */
  var requestListOfTypes = function() {
    chrome.send('requestListOfTypes');
  };

  return {
    makeTimer: makeTimer,
    dispatchEvent: dispatchEvent,
    events: new cr.EventTarget(),

    requestUpdatedAboutInfo: requestUpdatedAboutInfo,
    requestListOfTypes: requestListOfTypes,
  };
});
