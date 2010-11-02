// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains a bootstrap for the goog.* and chrome.*
 * APIs that are available to user scripts, when run inside of Firefox.  This
 * should be the first script injected into any page.
 *
 * @supported Firefox 3.x
 */

// Define the global namespaces.
var chrome = chrome || {};
var console = console || {};
var goog = goog || {};
var JSON = goog.json;
JSON.stringify = JSON.serialize;
var ceee = ceee || {
  /** @const */ API_EVENT_NAME_ : 'ceee-dom-api',
  /** @const */ API_ELEMENT_NAME_ : 'ceee-api-element',
  /** @const */ DATA_ATTRIBUTE_NAME_ : 'ceee-event-data',
  /** @const */ RETURN_ATTRIBUTE_NAME_ : 'ceee-event-return',

  /**
   * This object acts as the "chromeHidden" object for the chrome API
   * implementation.  Access to it is provided by the GetChromeHidden() to
   * emulate Chrome's behaviour.
   */
  hidden: {}
};

/**
 * Create and initialize a generic event for sending Google Chrome extension
 * API calls from the user script to the CEEE add-on.
 *
 * @return {!object} The new API event.
 * @private
 */
ceee.createApiEvent_ = function() {
  var evt = document.createEvent('Events');
  evt.initEvent(this.API_EVENT_NAME_,  // name of ceee API event type
                true,  // event should bubble up through the event chain
                false);  // event is not cancelable
  return evt;
};

/**
 * Get the DOM element in the document used to hold the arguments and
 * return value for Google Chrome user script extension API calls made to the
 * ceee add-on.  This element is added to the page DOM if not already
 * present, and is not visible.
 *
 * The DATA_ATTRIBUTE_NAME_ attributes of the returned element will be set to
 * the empty string upon return of this function.
 *
 * @param {!string} cmd The API command that will be fired by this event.
 * @return {!object} The DOM element.
 * @private
 */
ceee.getApiElement_ = function(cmd) {
  // Get the element from the DOM used to pass arguments receive results from
  // the ceee add-on.  If the element does not exist create it now.  There
  // should not be any threading issues with this object.
  var e = document.getElementById(this.API_ELEMENT_NAME_);
  if (e == null) {
    e = document.createElement(this.API_ELEMENT_NAME_);
    document.documentElement.appendChild(e);
  }

  // Remove any existing attributes that may be left over from a previous
  // API call.
  e.setAttribute(this.DATA_ATTRIBUTE_NAME_, '');
  e.setAttribute(this.RETURN_ATTRIBUTE_NAME_, '');

  // Set the command of the new API call, plus an id so that we can find
  // this element again.  We don't need to create a bunch of these.
  e.setAttribute('id', this.API_ELEMENT_NAME_);
  return e;
};

/**
 * Send a Google Chrome extension API call to the ceee add-on.
 *
 * @param {!string} cmd The API command that will be fired by this event.
 * @param {object} args Arguments of the API call.  Note that non-string
 *     entries in the array will be converted to string via toString().
 * @private
 */
ceee.sendMessage_ = function(cmd, args) {
  // Package up the DOM API call into the same format as the UI API.  Then
  // stuff it into the DOM API element.
  var data = {};
  data.name = cmd;
  data.args = args;

  var s = goog.json.serialize(data);
  var evt = this.createApiEvent_();
  var e = this.getApiElement_(cmd);

  e.setAttribute(this.DATA_ATTRIBUTE_NAME_, s);
  e.dispatchEvent(evt);

  // The dispatch call above is synchronous.  If there is a return value, it
  // has been placed in the DOM API element.  Extract it and return.
  var r = e.getAttribute(this.RETURN_ATTRIBUTE_NAME_);
  if (r && r.length > 0)
    return goog.json.parse(r);
};

// Add any functions that are missing from console.

if (!console.info) {
  /**
   * Write an informational log to the Firefox error console.
   * @param {string} message The message to log to the console.
   */
  console.info = function(message) {
    ceee.sendMessage_('LogToConsole', [message]);
  };
}

if (!console.log) {
  /**
   * Write an informational log to the Firefox error console.
   * @param {string} message The message to log to the console.
   */
  console.log = function(message) {
    ceee.sendMessage_('LogToConsole', [message]);
  };
}

if (!console.warn) {
  /**
   * Write a warning log to the Firefox error console.
   * @param {string} message The message to log to the console.
   */
  console.warn = function(message) {
    ceee.sendMessage_('LogToConsole', [message]);
  };
}

if (!console.error) {
  /**
   * Write an error to the Firefox error console.
   * @param {string} message The message to log to the console.
   */
  console.error = function(message) {
    ceee.sendMessage_('ErrorToConsole', [message]);
  };
}

if (!console.assert) {
  /**
   * Conditionally write an error to the Firefox error console.
   * @param {string} message The message to log to the console.
   */
  console.assert = function(test, message) {
    if (!test) {
      ceee.sendMessage_('ErrorToConsole', ['Assertion failed: ' + message]);
    }
  };
}

if (!console.count) {
  /**
   * Send a count message to the Firefox error console.
   * @param {string} name The name of the counter.
   */
  console.count = function(name) {
    var count = console.count.map[name];
    if (count) {
      count++;
    } else {
      count = 1;
    }
    console.count.map[name] = count;

    ceee.sendMessage_('LogToConsole', [name + ': ' + count]);
  };

  /**
   * A map of names to counts.
   * @type {Object.<string, number>}
   */
  console.count.map = {};
}

if (!console.time) {
  /**
   * Record a time message to be used with console.timeEnd().
   * @param {string} name The name of the timer.
   */
  console.time = function(name) {
    console.time.map[name] = new Date().getTime();
  };

  /**
   * A map of names to times in ms.
   * @type {Object.<string, number>}
   */
  console.time.map = {};
}

if (!console.timeEnd) {
  /**
   * Send a time end message to the Firefox error console.
   * @param {string} name The name of the timer.
   */
  console.timeEnd = function(name) {
    if (console.time.map && console.time.map[name]) {
      var diff = (new Date().getTime()) - console.time.map[name];
      ceee.sendMessage_('LogToConsole', [name + ': ' + diff + 'ms']);
    }
  };
}

ceee.OpenChannelToExtension = function(sourceId, targetId, name) {
  return ceee.sendMessage_('OpenChannelToExtension', [
    sourceId, targetId, name
  ]);
};

// The following methods are the public bindings required by the chrome
// javascript code implementation.  These binds hide the implementation details
// in FF, IE, and Chrome.

ceee.CloseChannel = function(portId) {
  return ceee.sendMessage_('CloseChannel', [portId]);
};

ceee.PortAddRef = function(portId) {
  return ceee.sendMessage_('PortAddRef', [portId]);
};

ceee.PortRelease = function(portId) {
  return ceee.sendMessage_('PortRelease', [portId]);
};

ceee.PostMessage = function(portId, msg) {
  return ceee.sendMessage_('PostMessage', [portId, msg]);
};

ceee.GetChromeHidden = function() {
  return ceee.hidden;
};

ceee.AttachEvent = function(eventName) {
  ceee.sendMessage_('AttachEvent', [eventName]);
};

ceee.DetachEvent = function(eventName) {
  ceee.sendMessage_('DetachEvent', [eventName]);
};
