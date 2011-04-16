// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome = chrome || {};
(function () {
  native function GetChromeHidden();
  native function AttachEvent(eventName);
  native function DetachEvent(eventName);

  var chromeHidden = GetChromeHidden();

  // Local implementation of JSON.parse & JSON.stringify that protect us
  // from being clobbered by an extension.
  chromeHidden.JSON = new (function() {
    var $Object = Object;
    var $Array = Array;
    var $jsonStringify = JSON.stringify;
    var $jsonParse = JSON.parse;

    this.stringify = function(thing) {
      var customizedObjectToJSON = $Object.prototype.toJSON;
      var customizedArrayToJSON = $Array.prototype.toJSON;
      if (customizedObjectToJSON !== undefined) {
        $Object.prototype.toJSON = null;
      }
      if (customizedArrayToJSON !== undefined) {
        $Array.prototype.toJSON = null;
      }
      try {
        return $jsonStringify(thing);
      } finally {
        if (customizedObjectToJSON !== undefined) {
          $Object.prototype.toJSON = customizedObjectToJSON;
        }
        if (customizedArrayToJSON !== undefined) {
          $Array.prototype.toJSON = customizedArrayToJSON;
        }
      }
    };

    this.parse = function(thing) {
      return $jsonParse(thing);
    };
  })();

  // Event object.  If opt_eventName is provided, this object represents
  // the unique instance of that named event, and dispatching an event
  // with that name will route through this object's listeners.
  //
  // Example:
  //   chrome.tabs.onChanged = new chrome.Event("tab-changed");
  //   chrome.tabs.onChanged.addListener(function(data) { alert(data); });
  //   chromeHidden.Event.dispatch("tab-changed", "hi");
  // will result in an alert dialog that says 'hi'.
  chrome.Event = function(opt_eventName, opt_argSchemas) {
    this.eventName_ = opt_eventName;
    this.listeners_ = [];

    // Validate event parameters if we are in debug.
    if (opt_argSchemas &&
        chromeHidden.validateCallbacks &&
        chromeHidden.validate) {

      this.validate_ = function(args) {
        try {
          chromeHidden.validate(args, opt_argSchemas);
        } catch (exception) {
          return "Event validation error during " + opt_eventName + " -- " +
                 exception;
        }
      };
    }
  };

  // A map of event names to the event object that is registered to that name.
  var attachedNamedEvents = {};

  // An array of all attached event objects, used for detaching on unload.
  var allAttachedEvents = [];

  chromeHidden.Event = {};

  // Dispatches a named event with the given JSON array, which is deserialized
  // before dispatch. The JSON array is the list of arguments that will be
  // sent with the event callback.
  chromeHidden.Event.dispatchJSON = function(name, args) {
    if (attachedNamedEvents[name]) {
      if (args) {
        args = chromeHidden.JSON.parse(args);
      }
      return attachedNamedEvents[name].dispatch.apply(
          attachedNamedEvents[name], args);
    }
  };

  // Dispatches a named event with the given arguments, supplied as an array.
  chromeHidden.Event.dispatch = function(name, args) {
    if (attachedNamedEvents[name]) {
      attachedNamedEvents[name].dispatch.apply(
          attachedNamedEvents[name], args);
    }
  };

  // Test if a named event has any listeners.
  chromeHidden.Event.hasListener = function(name) {
    return (attachedNamedEvents[name] &&
            attachedNamedEvents[name].listeners_.length > 0);
  };

  // Registers a callback to be called when this event is dispatched.
  chrome.Event.prototype.addListener = function(cb) {
    if (this.listeners_.length == 0) {
      this.attach_();
    }
    this.listeners_.push(cb);
  };

  // Unregisters a callback.
  chrome.Event.prototype.removeListener = function(cb) {
    var idx = this.findListener_(cb);
    if (idx == -1) {
      return;
    }

    this.listeners_.splice(idx, 1);
    if (this.listeners_.length == 0) {
      this.detach_();
    }
  };

  // Test if the given callback is registered for this event.
  chrome.Event.prototype.hasListener = function(cb) {
    return this.findListener_(cb) > -1;
  };

  // Test if any callbacks are registered for this event.
  chrome.Event.prototype.hasListeners = function(cb) {
    return this.listeners_.length > 0;
  };

  // Returns the index of the given callback if registered, or -1 if not
  // found.
  chrome.Event.prototype.findListener_ = function(cb) {
    for (var i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i] == cb) {
        return i;
      }
    }

    return -1;
  };

  // Dispatches this event object to all listeners, passing all supplied
  // arguments to this function each listener.
  chrome.Event.prototype.dispatch = function(varargs) {
    var args = Array.prototype.slice.call(arguments);
    if (this.validate_) {
      var validationErrors = this.validate_(args);
      if (validationErrors) {
        return validationErrors;
      }
    }
    for (var i = 0; i < this.listeners_.length; i++) {
      try {
        this.listeners_[i].apply(null, args);
      } catch (e) {
        console.error("Error in event handler for '" + this.eventName_ +
                      "': " + e);
      }
    }
  };

  // Attaches this event object to its name.  Only one object can have a given
  // name.
  chrome.Event.prototype.attach_ = function() {
    AttachEvent(this.eventName_);
    allAttachedEvents[allAttachedEvents.length] = this;
    if (!this.eventName_)
      return;

    if (attachedNamedEvents[this.eventName_]) {
      throw new Error("chrome.Event '" + this.eventName_ +
                      "' is already attached.");
    }

    attachedNamedEvents[this.eventName_] = this;
  };

  // Detaches this event object from its name.
  chrome.Event.prototype.detach_ = function() {
    var i = allAttachedEvents.indexOf(this);
    if (i >= 0)
      delete allAttachedEvents[i];
    DetachEvent(this.eventName_);
    if (!this.eventName_)
      return;

    if (!attachedNamedEvents[this.eventName_]) {
      throw new Error("chrome.Event '" + this.eventName_ +
                      "' is not attached.");
    }

    delete attachedNamedEvents[this.eventName_];
  };

  chrome.Event.prototype.destroy_ = function() {
    this.listeners_ = [];
    this.validate_ = [];
    this.detach_();
  };

  // Special load events: we don't use the DOM unload because that slows
  // down tab shutdown.  On the other hand, onUnload might not always fire,
  // since Chrome will terminate renderers on shutdown (SuddenTermination).
  chromeHidden.onLoad = new chrome.Event();
  chromeHidden.onUnload = new chrome.Event();

  chromeHidden.dispatchOnLoad = function(extensionId) {
    chromeHidden.onLoad.dispatch(extensionId);
  };

  chromeHidden.dispatchOnUnload = function() {
    chromeHidden.onUnload.dispatch();
    for (var i = 0; i < allAttachedEvents.length; ++i) {
      var event = allAttachedEvents[i];
      if (event)
        event.detach_();
    }
  };

  chromeHidden.dispatchError = function(msg) {
    console.error(msg);
  };
})();
