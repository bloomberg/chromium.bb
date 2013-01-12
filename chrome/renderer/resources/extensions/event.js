// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  var DCHECK = requireNative('logging').DCHECK;
  var eventBindingsNatives = requireNative('event_bindings');
  var AttachEvent = eventBindingsNatives.AttachEvent;
  var DetachEvent = eventBindingsNatives.DetachEvent;
  var AttachFilteredEvent = eventBindingsNatives.AttachFilteredEvent;
  var DetachFilteredEvent = eventBindingsNatives.DetachFilteredEvent;
  var MatchAgainstEventFilter = eventBindingsNatives.MatchAgainstEventFilter;
  var sendRequest = require('sendRequest').sendRequest;
  var utils = require('utils');
  var validate = require('schemaUtils').validate;

  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
  var GetExtensionAPIDefinition =
      requireNative('apiDefinitions').GetExtensionAPIDefinition;

  // Schemas for the rule-style functions on the events API that
  // only need to be generated occasionally, so populate them lazily.
  var ruleFunctionSchemas = {
    // These values are set lazily:
    // addRules: {},
    // getRules: {},
    // removeRules: {}
  };

  // This function ensures that |ruleFunctionSchemas| is populated.
  function ensureRuleSchemasLoaded() {
    if (ruleFunctionSchemas.addRules)
      return;
    var eventsSchema = GetExtensionAPIDefinition("events")[0];
    var eventType = utils.lookup(eventsSchema.types, 'id', 'events.Event');

    ruleFunctionSchemas.addRules =
        utils.lookup(eventType.functions, 'name', 'addRules');
    ruleFunctionSchemas.getRules =
        utils.lookup(eventType.functions, 'name', 'getRules');
    ruleFunctionSchemas.removeRules =
        utils.lookup(eventType.functions, 'name', 'removeRules');
  }

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

  // A map of event names to the event object that is registered to that name.
  var attachedNamedEvents = {};

  // An array of all attached event objects, used for detaching on unload.
  var allAttachedEvents = [];

  // A map of functions that massage event arguments before they are dispatched.
  // Key is event name, value is function.
  var eventArgumentMassagers = {};

  // Handles adding/removing/dispatching listeners for unfiltered events.
  var UnfilteredAttachmentStrategy = function(event) {
    this.event_ = event;
  };

  UnfilteredAttachmentStrategy.prototype.onAddedListener =
      function(listener) {
    // Only attach / detach on the first / last listener removed.
    if (this.event_.listeners_.length == 0)
      AttachEvent(this.event_.eventName_);
  };

  UnfilteredAttachmentStrategy.prototype.onRemovedListener =
      function(listener) {
    if (this.event_.listeners_.length == 0)
      this.detach(true);
  };

  UnfilteredAttachmentStrategy.prototype.detach = function(manual) {
    DetachEvent(this.event_.eventName_, manual);
  };

  UnfilteredAttachmentStrategy.prototype.getListenersByIDs = function(ids) {
    return this.event_.listeners_;
  };

  var FilteredAttachmentStrategy = function(event) {
    this.event_ = event;
    this.listenerMap_ = {};
  };

  FilteredAttachmentStrategy.idToEventMap = {};

  FilteredAttachmentStrategy.prototype.onAddedListener = function(listener) {
    var id = AttachFilteredEvent(this.event_.eventName_,
        listener.filters || {});
    if (id == -1)
      throw new Error("Can't add listener");
    listener.id = id;
    this.listenerMap_[id] = listener;
    FilteredAttachmentStrategy.idToEventMap[id] = this.event_;
  };

  FilteredAttachmentStrategy.prototype.onRemovedListener = function(listener) {
    this.detachListener(listener, true);
  };

  FilteredAttachmentStrategy.prototype.detachListener =
      function(listener, manual) {
    if (listener.id == undefined)
      throw new Error("listener.id undefined - '" + listener + "'");
    var id = listener.id;
    delete this.listenerMap_[id];
    delete FilteredAttachmentStrategy.idToEventMap[id];
    DetachFilteredEvent(id, manual);
  };

  FilteredAttachmentStrategy.prototype.detach = function(manual) {
    for (var i in this.listenerMap_)
      this.detachListener(this.listenerMap_[i], manual);
  };

  FilteredAttachmentStrategy.prototype.getListenersByIDs = function(ids) {
    var result = [];
    for (var i = 0; i < ids.length; i++)
      result.push(this.listenerMap_[ids[i]]);
    return result;
  };

  chromeHidden.parseEventOptions = function(opt_eventOptions) {
    function merge(dest, src) {
      for (var k in src) {
        if (!dest.hasOwnProperty(k)) {
          dest[k] = src[k];
        }
      }
    }

    var options = opt_eventOptions || {};
    merge(options,
        {supportsFilters: false,
         supportsListeners: true,
         supportsRules: false,
        });
    return options;
  };

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
    this.eventOptions_ = chromeHidden.parseEventOptions(opt_eventOptions);

    if (this.eventOptions_.supportsRules && !opt_eventName)
      throw new Error("Events that support rules require an event name.");

    if (this.eventOptions_.supportsFilters) {
      this.attachmentStrategy_ = new FilteredAttachmentStrategy(this);
    } else {
      this.attachmentStrategy_ = new UnfilteredAttachmentStrategy(this);
    }

    // Validate event arguments (the data that is passed to the callbacks)
    // if we are in debug.
    if (opt_argSchemas &&
        chromeHidden.validateCallbacks) {

      this.validateEventArgs_ = function(args) {
        try {
          validate(args, opt_argSchemas);
        } catch (exception) {
          return "Event validation error during " + opt_eventName + " -- " +
                 exception;
        }
      };
    } else {
      this.validateEventArgs_ = function() {}
    }
  };


  chromeHidden.Event = {};

  // callback is a function(args, dispatch). args are the args we receive from
  // dispatchEvent(), and dispatch is a function(args) that dispatches args to
  // its listeners.
  chromeHidden.Event.registerArgumentMassager = function(name, callback) {
    if (eventArgumentMassagers[name])
      throw new Error("Massager already registered for event: " + name);
    eventArgumentMassagers[name] = callback;
  };

  // Dispatches a named event with the given argument array. The args array is
  // the list of arguments that will be sent to the event callback.
  chromeHidden.Event.dispatchEvent = function(name, args, filteringInfo) {
    var listenerIDs = null;

    if (filteringInfo)
      listenerIDs = MatchAgainstEventFilter(name, filteringInfo);

    var event = attachedNamedEvents[name];
    if (!event)
      return;

    var dispatchArgs = function(args) {
      result = event.dispatch_(args, listenerIDs);
      if (result)
        DCHECK(!result.validationErrors, result.validationErrors);
    };

    if (eventArgumentMassagers[name])
      eventArgumentMassagers[name](args, dispatchArgs);
    else
      dispatchArgs(args);
  };

  // Test if a named event has any listeners.
  chromeHidden.Event.hasListener = function(name) {
    return (attachedNamedEvents[name] &&
            attachedNamedEvents[name].listeners_.length > 0);
  };

  // Registers a callback to be called when this event is dispatched.
  chrome.Event.prototype.addListener = function(cb, filters) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    if (this.eventOptions_.maxListeners &&
        this.getListenerCount() >= this.eventOptions_.maxListeners)
      throw new Error("Too many listeners for " + this.eventName_);
    if (filters) {
      if (!this.eventOptions_.supportsFilters)
        throw new Error("This event does not support filters.");
      if (filters.url && !(filters.url instanceof Array))
        throw new Error("filters.url should be an array");
    }
    var listener = {callback: cb, filters: filters};
    this.attach_(listener);
    this.listeners_.push(listener);
  };

  chrome.Event.prototype.attach_ = function(listener) {
    this.attachmentStrategy_.onAddedListener(listener);
    if (this.listeners_.length == 0) {
      allAttachedEvents[allAttachedEvents.length] = this;
      if (!this.eventName_)
        return;

      if (attachedNamedEvents[this.eventName_]) {
        throw new Error("chrome.Event '" + this.eventName_ +
                        "' is already attached.");
      }

      attachedNamedEvents[this.eventName_] = this;
    }
  };

  // Unregisters a callback.
  chrome.Event.prototype.removeListener = function(cb) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    var idx = this.findListener_(cb);
    if (idx == -1) {
      return;
    }

    var removedListener = this.listeners_.splice(idx, 1)[0];
    this.attachmentStrategy_.onRemovedListener(removedListener);

    if (this.listeners_.length == 0) {
      var i = allAttachedEvents.indexOf(this);
      if (i >= 0)
        delete allAttachedEvents[i];
      if (!this.eventName_)
        return;

      if (!attachedNamedEvents[this.eventName_]) {
        throw new Error("chrome.Event '" + this.eventName_ +
                        "' is not attached.");
      }

      delete attachedNamedEvents[this.eventName_];
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
    return this.getListenerCount() > 0;
  };

  // Return the number of listeners on this event.
  chrome.Event.prototype.getListenerCount = function() {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    return this.listeners_.length;
  };

  // Returns the index of the given callback if registered, or -1 if not
  // found.
  chrome.Event.prototype.findListener_ = function(cb) {
    for (var i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i].callback == cb) {
        return i;
      }
    }

    return -1;
  };

  chrome.Event.prototype.dispatch_ = function(args, listenerIDs) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    var validationErrors = this.validateEventArgs_(args);
    if (validationErrors) {
      console.error(validationErrors);
      return {validationErrors: validationErrors};
    }

    // Make a copy of the listeners in case the listener list is modified
    // while dispatching the event.
    var listeners =
        this.attachmentStrategy_.getListenersByIDs(listenerIDs).slice();

    var results = [];
    for (var i = 0; i < listeners.length; i++) {
      try {
        var result = this.dispatchToListener(listeners[i].callback, args);
        if (result !== undefined)
          results.push(result);
      } catch (e) {
        console.error("Error in event handler for '" + this.eventName_ +
                      "': " + e.message + ' ' + e.stack);
      }
    }
    if (results.length)
      return {results: results};
  }

  // Can be overridden to support custom dispatching.
  chrome.Event.prototype.dispatchToListener = function(callback, args) {
    return callback.apply(null, args);
  }

  // Dispatches this event object to all listeners, passing all supplied
  // arguments to this function each listener.
  chrome.Event.prototype.dispatch = function(varargs) {
    return this.dispatch_(Array.prototype.slice.call(arguments), undefined);
  };

  // Detaches this event object from its name.
  chrome.Event.prototype.detach_ = function() {
    this.attachmentStrategy_.detach(false);
  };

  chrome.Event.prototype.destroy_ = function() {
    this.listeners_ = [];
    this.validateEventArgs_ = [];
    this.detach_(false);
  };

  chrome.Event.prototype.addRules = function(rules, opt_cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");

    // Takes a list of JSON datatype identifiers and returns a schema fragment
    // that verifies that a JSON object corresponds to an array of only these
    // data types.
    function buildArrayOfChoicesSchema(typesList) {
      return {
        'type': 'array',
        'items': {
          'choices': typesList.map(function(el) {return {'$ref': el};})
        }
      };
    };

    // Validate conditions and actions against specific schemas of this
    // event object type.
    // |rules| is an array of JSON objects that follow the Rule type of the
    // declarative extension APIs. |conditions| is an array of JSON type
    // identifiers that are allowed to occur in the conditions attribute of each
    // rule. Likewise, |actions| is an array of JSON type identifiers that are
    // allowed to occur in the actions attribute of each rule.
    function validateRules(rules, conditions, actions) {
      var conditionsSchema = buildArrayOfChoicesSchema(conditions);
      var actionsSchema = buildArrayOfChoicesSchema(actions);
      rules.forEach(function(rule) {
        validate([rule.conditions], [conditionsSchema]);
        validate([rule.actions], [actionsSchema]);
      })
    };

    if (!this.eventOptions_.conditions || !this.eventOptions_.actions) {
      throw new Error('Event ' + this.eventName_ + ' misses conditions or ' +
                'actions in the API specification.');
    }

    validateRules(rules,
                  this.eventOptions_.conditions,
                  this.eventOptions_.actions);

    ensureRuleSchemasLoaded();
    // We remove the first parameter from the validation to give the user more
    // meaningful error messages.
    validate([rules, opt_cb],
             ruleFunctionSchemas.addRules.parameters.slice().splice(1));
    sendRequest("events.addRules", [this.eventName_, rules, opt_cb],
                ruleFunctionSchemas.addRules.parameters);
  }

  chrome.Event.prototype.removeRules = function(ruleIdentifiers, opt_cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    ensureRuleSchemasLoaded();
    // We remove the first parameter from the validation to give the user more
    // meaningful error messages.
    validate([ruleIdentifiers, opt_cb],
             ruleFunctionSchemas.removeRules.parameters.slice().splice(1));
    sendRequest("events.removeRules",
                [this.eventName_, ruleIdentifiers, opt_cb],
                ruleFunctionSchemas.removeRules.parameters);
  }

  chrome.Event.prototype.getRules = function(ruleIdentifiers, cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    ensureRuleSchemasLoaded();
    // We remove the first parameter from the validation to give the user more
    // meaningful error messages.
    validate([ruleIdentifiers, cb],
             ruleFunctionSchemas.getRules.parameters.slice().splice(1));

    sendRequest("events.getRules",
                [this.eventName_, ruleIdentifiers, cb],
                ruleFunctionSchemas.getRules.parameters);
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
        event.detach_();
    }
  };

  chromeHidden.dispatchError = function(msg) {
    console.error(msg);
  };

  exports.Event = chrome.Event;
