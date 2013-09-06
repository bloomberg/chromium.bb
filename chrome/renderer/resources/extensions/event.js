// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  var eventNatives = requireNative('event_natives');
  var logging = requireNative('logging');
  var schemaRegistry = requireNative('schema_registry');
  var sendRequest = require('sendRequest').sendRequest;
  var utils = require('utils');
  var validate = require('schemaUtils').validate;
  var unloadEvent = require('unload_event');

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
    var eventsSchema = schemaRegistry.GetSchema("events");
    var eventType = utils.lookup(eventsSchema.types, 'id', 'events.Event');

    ruleFunctionSchemas.addRules =
        utils.lookup(eventType.functions, 'name', 'addRules');
    ruleFunctionSchemas.getRules =
        utils.lookup(eventType.functions, 'name', 'getRules');
    ruleFunctionSchemas.removeRules =
        utils.lookup(eventType.functions, 'name', 'removeRules');
  }

  // A map of event names to the event object that is registered to that name.
  var attachedNamedEvents = {};

  // An array of all attached event objects, used for detaching on unload.
  var allAttachedEvents = [];

  // A map of functions that massage event arguments before they are dispatched.
  // Key is event name, value is function.
  var eventArgumentMassagers = {};

  // An attachment strategy for events that aren't attached to the browser.
  // This applies to events with the "unmanaged" option and events without
  // names.
  var NullAttachmentStrategy = function(event) {
    this.event_ = event;
  };
  NullAttachmentStrategy.prototype.onAddedListener =
      function(listener) {
  };
  NullAttachmentStrategy.prototype.onRemovedListener =
      function(listener) {
  };
  NullAttachmentStrategy.prototype.detach = function(manual) {
  };
  NullAttachmentStrategy.prototype.getListenersByIDs = function(ids) {
    // |ids| is for filtered events only.
    return this.event_.listeners_;
  };

  // Handles adding/removing/dispatching listeners for unfiltered events.
  var UnfilteredAttachmentStrategy = function(event) {
    this.event_ = event;
  };

  UnfilteredAttachmentStrategy.prototype.onAddedListener =
      function(listener) {
    // Only attach / detach on the first / last listener removed.
    if (this.event_.listeners_.length == 0)
      eventNatives.AttachEvent(this.event_.eventName_);
  };

  UnfilteredAttachmentStrategy.prototype.onRemovedListener =
      function(listener) {
    if (this.event_.listeners_.length == 0)
      this.detach(true);
  };

  UnfilteredAttachmentStrategy.prototype.detach = function(manual) {
    eventNatives.DetachEvent(this.event_.eventName_, manual);
  };

  UnfilteredAttachmentStrategy.prototype.getListenersByIDs = function(ids) {
    // |ids| is for filtered events only.
    return this.event_.listeners_;
  };

  var FilteredAttachmentStrategy = function(event) {
    this.event_ = event;
    this.listenerMap_ = {};
  };

  FilteredAttachmentStrategy.idToEventMap = {};

  FilteredAttachmentStrategy.prototype.onAddedListener = function(listener) {
    var id = eventNatives.AttachFilteredEvent(this.event_.eventName_,
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
    eventNatives.DetachFilteredEvent(id, manual);
  };

  FilteredAttachmentStrategy.prototype.detach = function(manual) {
    for (var i in this.listenerMap_)
      this.detachListener(this.listenerMap_[i], manual);
  };

  FilteredAttachmentStrategy.prototype.getListenersByIDs = function(ids) {
    var result = [];
    for (var i = 0; i < ids.length; i++)
      $Array.push(result, this.listenerMap_[ids[i]]);
    return result;
  };

  function parseEventOptions(opt_eventOptions) {
    function merge(dest, src) {
      for (var k in src) {
        if (!$Object.hasOwnProperty(dest, k)) {
          dest[k] = src[k];
        }
      }
    }

    var options = opt_eventOptions || {};
    merge(options, {
      // Event supports adding listeners with filters ("filtered events"), for
      // example as used in the webNavigation API.
      //
      // event.addListener(listener, [filter1, filter2]);
      supportsFilters: false,

      // Events supports vanilla events. Most APIs use these.
      //
      // event.addListener(listener);
      supportsListeners: true,

      // Event supports adding rules ("declarative events") rather than
      // listeners, for example as used in the declarativeWebRequest API.
      //
      // event.addRules([rule1, rule2]);
      supportsRules: false,

      // Event is unmanaged in that the browser has no knowledge of its
      // existence; it's never invoked, doesn't keep the renderer alive, and
      // the bindings system has no knowledge of it.
      //
      // Both events created by user code (new chrome.Event()) and messaging
      // events are unmanaged, though in the latter case the browser *does*
      // interact indirectly with them via IPCs written by hand.
      unmanaged: false,
    });
    return options;
  };

  // Event object.  If opt_eventName is provided, this object represents
  // the unique instance of that named event, and dispatching an event
  // with that name will route through this object's listeners. Note that
  // opt_eventName is required for events that support rules.
  //
  // Example:
  //   var Event = require('event_bindings').Event;
  //   chrome.tabs.onChanged = new Event("tab-changed");
  //   chrome.tabs.onChanged.addListener(function(data) { alert(data); });
  //   Event.dispatch("tab-changed", "hi");
  // will result in an alert dialog that says 'hi'.
  //
  // If opt_eventOptions exists, it is a dictionary that contains the boolean
  // entries "supportsListeners" and "supportsRules".
  var Event = function(opt_eventName, opt_argSchemas, opt_eventOptions) {
    this.eventName_ = opt_eventName;
    this.argSchemas_ = opt_argSchemas;
    this.listeners_ = [];
    this.eventOptions_ = parseEventOptions(opt_eventOptions);

    if (!this.eventName_) {
      if (this.eventOptions_.supportsRules)
        throw new Error("Events that support rules require an event name.");
      // Events without names cannot be managed by the browser by definition
      // (the browser has no way of identifying them).
      this.eventOptions_.unmanaged = true;
    }

    // Track whether the event has been destroyed to help track down the cause
    // of http://crbug.com/258526.
    // This variable will eventually hold the stack trace of the destroy call.
    // TODO(kalman): Delete this and replace with more sound logic that catches
    // when events are used without being *attached*.
    this.destroyed_ = null;

    if (this.eventOptions_.unmanaged)
      this.attachmentStrategy_ = new NullAttachmentStrategy(this);
    else if (this.eventOptions_.supportsFilters)
      this.attachmentStrategy_ = new FilteredAttachmentStrategy(this);
    else
      this.attachmentStrategy_ = new UnfilteredAttachmentStrategy(this);
  };

  // callback is a function(args, dispatch). args are the args we receive from
  // dispatchEvent(), and dispatch is a function(args) that dispatches args to
  // its listeners.
  function registerArgumentMassager(name, callback) {
    if (eventArgumentMassagers[name])
      throw new Error("Massager already registered for event: " + name);
    eventArgumentMassagers[name] = callback;
  }

  // Dispatches a named event with the given argument array. The args array is
  // the list of arguments that will be sent to the event callback.
  function dispatchEvent(name, args, filteringInfo) {
    var listenerIDs = null;

    if (filteringInfo)
      listenerIDs = eventNatives.MatchAgainstEventFilter(name, filteringInfo);

    var event = attachedNamedEvents[name];
    if (!event)
      return;

    var dispatchArgs = function(args) {
      var result = event.dispatch_(args, listenerIDs);
      if (result)
        logging.DCHECK(!result.validationErrors, result.validationErrors);
      return result;
    };

    if (eventArgumentMassagers[name])
      eventArgumentMassagers[name](args, dispatchArgs);
    else
      dispatchArgs(args);
  }

  // Registers a callback to be called when this event is dispatched.
  Event.prototype.addListener = function(cb, filters) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    if (this.eventOptions_.maxListeners &&
        this.getListenerCount() >= this.eventOptions_.maxListeners) {
      throw new Error("Too many listeners for " + this.eventName_);
    }
    if (filters) {
      if (!this.eventOptions_.supportsFilters)
        throw new Error("This event does not support filters.");
      if (filters.url && !(filters.url instanceof Array))
        throw new Error("filters.url should be an array");
    }
    var listener = {callback: cb, filters: filters};
    this.attach_(listener);
    $Array.push(this.listeners_, listener);
  };

  Event.prototype.attach_ = function(listener) {
    this.attachmentStrategy_.onAddedListener(listener);

    if (this.listeners_.length == 0) {
      allAttachedEvents[allAttachedEvents.length] = this;
      if (this.eventName_) {
        if (attachedNamedEvents[this.eventName_]) {
          throw new Error("Event '" + this.eventName_ +
                          "' is already attached.");
        }
        attachedNamedEvents[this.eventName_] = this;
      }
    }
  };

  // Unregisters a callback.
  Event.prototype.removeListener = function(cb) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");

    var idx = this.findListener_(cb);
    if (idx == -1)
      return;

    var removedListener = $Array.splice(this.listeners_, idx, 1)[0];
    this.attachmentStrategy_.onRemovedListener(removedListener);

    if (this.listeners_.length == 0) {
      var i = allAttachedEvents.indexOf(this);
      if (i >= 0)
        delete allAttachedEvents[i];
      if (this.eventName_) {
        if (!attachedNamedEvents[this.eventName_])
          throw new Error("Event '" + this.eventName_ + "' is not attached.");
        delete attachedNamedEvents[this.eventName_];
      }
    }
  };

  // Test if the given callback is registered for this event.
  Event.prototype.hasListener = function(cb) {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    return this.findListener_(cb) > -1;
  };

  // Test if any callbacks are registered for this event.
  Event.prototype.hasListeners = function() {
    return this.getListenerCount() > 0;
  };

  // Return the number of listeners on this event.
  Event.prototype.getListenerCount = function() {
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");
    return this.listeners_.length;
  };

  // Returns the index of the given callback if registered, or -1 if not
  // found.
  Event.prototype.findListener_ = function(cb) {
    for (var i = 0; i < this.listeners_.length; i++) {
      if (this.listeners_[i].callback == cb) {
        return i;
      }
    }

    return -1;
  };

  Event.prototype.dispatch_ = function(args, listenerIDs) {
    if (this.destroyed_) {
      throw new Error(this.eventName_ + ' was already destroyed at: ' +
                      this.destroyed_);
    }
    if (!this.eventOptions_.supportsListeners)
      throw new Error("This event does not support listeners.");

    if (this.argSchemas_ && logging.DCHECK_IS_ON()) {
      try {
        validate(args, this.argSchemas_);
      } catch (e) {
        e.message += ' in ' + this.eventName_;
        throw e;
      }
    }

    // Make a copy of the listeners in case the listener list is modified
    // while dispatching the event.
    var listeners = $Array.slice(
        this.attachmentStrategy_.getListenersByIDs(listenerIDs));

    var results = [];
    for (var i = 0; i < listeners.length; i++) {
      try {
        var result = this.dispatchToListener(listeners[i].callback, args);
        if (result !== undefined)
          $Array.push(results, result);
      } catch (e) {
        console.error('Error in event handler for ' +
                      (this.eventName_ ? this.eventName_ : '(unknown)') +
                      ': ' + e.stack);
      }
    }
    if (results.length)
      return {results: results};
  }

  // Can be overridden to support custom dispatching.
  Event.prototype.dispatchToListener = function(callback, args) {
    return $Function.apply(callback, null, args);
  }

  // Dispatches this event object to all listeners, passing all supplied
  // arguments to this function each listener.
  Event.prototype.dispatch = function(varargs) {
    return this.dispatch_($Array.slice(arguments), undefined);
  };

  // Detaches this event object from its name.
  Event.prototype.detach_ = function() {
    this.attachmentStrategy_.detach(false);
  };

  Event.prototype.destroy_ = function() {
    this.listeners_.length = 0;
    this.detach_();
    this.destroyed_ = new Error().stack;
  };

  Event.prototype.addRules = function(rules, opt_cb) {
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
      $Array.forEach(rules, function(rule) {
        validate([rule.conditions], [conditionsSchema]);
        validate([rule.actions], [actionsSchema]);
      });
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
             $Array.splice(
                 $Array.slice(ruleFunctionSchemas.addRules.parameters), 1));
    sendRequest("events.addRules", [this.eventName_, rules, opt_cb],
                ruleFunctionSchemas.addRules.parameters);
  }

  Event.prototype.removeRules = function(ruleIdentifiers, opt_cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    ensureRuleSchemasLoaded();
    // We remove the first parameter from the validation to give the user more
    // meaningful error messages.
    validate([ruleIdentifiers, opt_cb],
             $Array.splice(
                 $Array.slice(ruleFunctionSchemas.removeRules.parameters), 1));
    sendRequest("events.removeRules",
                [this.eventName_, ruleIdentifiers, opt_cb],
                ruleFunctionSchemas.removeRules.parameters);
  }

  Event.prototype.getRules = function(ruleIdentifiers, cb) {
    if (!this.eventOptions_.supportsRules)
      throw new Error("This event does not support rules.");
    ensureRuleSchemasLoaded();
    // We remove the first parameter from the validation to give the user more
    // meaningful error messages.
    validate([ruleIdentifiers, cb],
             $Array.splice(
                 $Array.slice(ruleFunctionSchemas.getRules.parameters), 1));

    sendRequest("events.getRules",
                [this.eventName_, ruleIdentifiers, cb],
                ruleFunctionSchemas.getRules.parameters);
  }

  unloadEvent.addListener(function() {
    for (var i = 0; i < allAttachedEvents.length; ++i) {
      var event = allAttachedEvents[i];
      if (event)
        event.detach_();
    }
  });

  // NOTE: Event is (lazily) exposed as chrome.Event from dispatcher.cc.
  exports.Event = Event;

  exports.dispatchEvent = dispatchEvent;
  exports.parseEventOptions = parseEventOptions;
  exports.registerArgumentMassager = registerArgumentMassager;
