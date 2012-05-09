// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  var eventBindingsNatives = requireNative('event_bindings');
  var AttachEvent = eventBindingsNatives.AttachEvent;
  var DetachEvent = eventBindingsNatives.DetachEvent;
  var Print = eventBindingsNatives.Print;

  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

  // Local implementation of JSON.parse & JSON.stringify that protect us
  // from being clobbered by an extension.
  //
  // TODO(aa): This makes me so sad. We shouldn't need it, as we can just pass
  // Values directly over IPC without serializing to strings and use
  // JSONValueConverter.
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
  // with that name will route through this object's listeners. Note that
  // opt_eventName is required for events that support rules.
  //
  // Example:
  //   chrome.tabs.onChanged = new chrome.Event("tab-changed");
  //   chrome.tabs.onChanged.addListener(function(data) { alert(data); });
  //   chromeHidden.Event.dispatch("tab-changed", "hi");
  // will result in an alert dialog that says 'hi'.
  //
  // If opt_eventOptions exists, it is a dictionary that contains the boolean
  // entries "supportsListeners" and "supportsRules".
  chrome.Event = function(opt_eventName, opt_argSchemas, opt_eventOptions) {
    this.eventName_ = opt_eventName;
    this.listeners_ = [];
    this.eventOptions_ = opt_eventOptions ||
        {"supportsListeners": true, "supportsRules": false};

    if (this.eventOptions_.supportsRules && !opt_eventName)
      throw new Error("Events that support rules require an event name.");

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
    } else {
      this.validate_ = function() {}
    }
  };

  // A map of event names to the event object that is registered to that name.
  var attachedNamedEvents = {};

  // An array of all attached event objects, used for detaching on unload.
  var allAttachedEvents = [];

  // A map of functions that massage event arguments before they are dispatched.
  // Key is event name, value is function.
  var eventArgumentMassagers = {};

  chromeHidden.Event = {};

  chromeHidden.Event.registerArgumentMassager = function(name, fn) {
    if (eventArgumentMassagers[name])
      throw new Error("Massager already registered for event: " + name);
    eventArgumentMassagers[name] = fn;
  };

  // Dispatches a named event with the given JSON array, which is deserialized
  // before dispatch. The JSON array is the list of arguments that will be
  // sent with the event callback.
  chromeHidden.Event.dispatchJSON = function(name, args) {
    if (attachedNamedEvents[name]) {
      if (args) {
        args = chromeHidden.JSON.parse(args);
        if (eventArgumentMassagers[name])
          eventArgumentMassagers[name](args);
      }
      var result = attachedNamedEvents[name].dispatch.apply(
          attachedNamedEvents[name], args);
      if (result && result.validationErrors)
        return result.validationErrors;
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
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    if (this.listeners_.length == 0) {
      this.attach_();
    }
    this.listeners_.push(cb);
  };

  // Unregisters a callback.
  chrome.Event.prototype.removeListener = function(cb) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    var idx = this.findListener_(cb);
    if (idx == -1) {
      return;
    }

    this.listeners_.splice(idx, 1);
    if (this.listeners_.length == 0) {
      this.detach_(true);
    }
  };

  // Test if the given callback is registered for this event.
  chrome.Event.prototype.hasListener = function(cb) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    return this.findListener_(cb) > -1;
  };

  // Test if any callbacks are registered for this event.
  chrome.Event.prototype.hasListeners = function() {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
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
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    var args = Array.prototype.slice.call(arguments);
    var validationErrors = this.validate_(args);
    if (validationErrors) {
      return {validationErrors: validationErrors};
    }
    var results = [];
    for (var i = 0; i < this.listeners_.length; i++) {
      try {
        var result = this.listeners_[i].apply(null, args);
        if (result !== undefined)
          results.push(result);
      } catch (e) {
        console.error("Error in event handler for '" + this.eventName_ +
                      "': " + e.message + ' ' + e.stack);
      }
    }
    if (results.length)
      return {results: results};
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
  chrome.Event.prototype.detach_ = function(manual) {
    var i = allAttachedEvents.indexOf(this);
    if (i >= 0)
      delete allAttachedEvents[i];
    DetachEvent(this.eventName_, manual);
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
    this.detach_(false);
  };

  // Gets the declarative API object, or undefined if this extension doesn't
  // have access to it.
  //
  // This is defined as a function (rather than a variable) because it isn't
  // accessible until the schema bindings have been generated.
  function getDeclarativeAPI() {
    return chromeHidden.internalAPIs.declarative;
  }

  chrome.Event.prototype.addRules = function(rules, opt_cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    if (!getDeclarativeAPI()) {
      throw new Error("You must have permission to use the declarative " +
                      "API to support rules in events");
    }
    getDeclarativeAPI().addRules(this.eventName_, rules, opt_cb);
  }

  chrome.Event.prototype.removeRules = function(ruleIdentifiers, opt_cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    if (!getDeclarativeAPI()) {
      throw new Error("You must have permission to use the declarative " +
                      "API to support rules in events");
    }
    getDeclarativeAPI().removeRules(
        this.eventName_, ruleIdentifiers, opt_cb);
  }

  chrome.Event.prototype.getRules = function(ruleIdentifiers, cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    if (!getDeclarativeAPI()) {
      throw new Error("You must have permission to use the declarative " +
                      "API to support rules in events");
    }
    getDeclarativeAPI().getRules(
        this.eventName_, ruleIdentifiers, cb);
  }

  // Special load events: we don't use the DOM unload because that slows
  // down tab shutdown.  On the other hand, onUnload might not always fire,
  // since Chrome will terminate renderers on shutdown (SuddenTermination).
  chromeHidden.onLoad = new chrome.Event();
  chromeHidden.onUnload = new chrome.Event();

  chromeHidden.dispatchOnLoad =
      chromeHidden.onLoad.dispatch.bind(chromeHidden.onLoad);

  chromeHidden.dispatchOnUnload = function() {
    chromeHidden.onUnload.dispatch();
    for (var i = 0; i < allAttachedEvents.length; ++i) {
      var event = allAttachedEvents[i];
      if (event)
        event.detach_(false);
    }
  };

  chromeHidden.dispatchError = function(msg) {
    console.error(msg);
  };
