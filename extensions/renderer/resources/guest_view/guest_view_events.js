// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event management for GuestViewContainers.

var EventBindings = require('event_bindings');

var CreateEvent = function(name) {
  var eventOpts = {supportsListeners: true, supportsFilters: true};
  return new EventBindings.Event(name, undefined, eventOpts);
};

function GuestViewEvents(view) {
  this.view = view;
  this.on = {};

  // |setupEventProperty| is normally called automatically, but these events are
  // are registered here because they are dispatched from GuestViewContainer
  // instead of in response to extension events.
  this.setupEventProperty('contentresize');
  this.setupEventProperty('resize');
  this.setupEvents();
}

// |GuestViewEvents.EVENTS| is a dictionary of extension events to be listened
//     for, which specifies how each event should be handled. The events are
//     organized by name, and by default will be dispatched as DOM events with
//     the same name.
// |cancelable| (default: false) specifies whether the DOM event's default
//     behavior can be canceled. If the default action associated with the event
//     is prevented, then its dispatch function will return false in its event
//     handler. The event must have a specified |handler| for this to be
//     meaningful.
// |evt| specifies a descriptor object for the extension event. An event
//     listener will be attached to this descriptor.
// |fields| (default: none) specifies the public-facing fields in the DOM event
//     that are accessible to developers.
// |handler| specifies the name of a handler function to be called each time
//     that extension event is caught by its event listener. The DOM event
//     should be dispatched within this handler function (if desired). With no
//     handler function, the DOM event will be dispatched by default each time
//     the extension event is caught.
// |internal| (default: false) specifies that the event will not be dispatched
//     as a DOM event, and will also not appear as an on* property on the view’s
//     element. A |handler| should be specified for all internal events, and
//     |fields| and |cancelable| should be left unspecified (as they are only
//     meaningful for DOM events).
GuestViewEvents.EVENTS = {};

// Sets up the handling of events.
GuestViewEvents.prototype.setupEvents = function() {
  for (var eventName in GuestViewEvents.EVENTS) {
    this.setupEvent(eventName, GuestViewEvents.EVENTS[eventName]);
  }

  var events = this.getEvents();
  for (var eventName in events) {
    this.setupEvent(eventName, events[eventName]);
  }
};

// Sets up the handling of the |eventName| event.
GuestViewEvents.prototype.setupEvent = function(eventName, eventInfo) {
  if (!eventInfo.internal) {
    this.setupEventProperty(eventName);
  }

  var listenerOpts = { instanceId: this.view.viewInstanceId };
  if (eventInfo.handler) {
    eventInfo.evt.addListener(function(e) {
      this[eventInfo.handler](e, eventName);
    }.bind(this), listenerOpts);
    return;
  }

  // Internal events are not dispatched as DOM events.
  if (eventInfo.internal) {
    return;
  }

  eventInfo.evt.addListener(function(e) {
    var domEvent = this.makeDomEvent(e, eventName);
    this.view.dispatchEvent(domEvent);
  }.bind(this), listenerOpts);
};

// Constructs a DOM event based on the info for the |eventName| event provided
// in either |GuestViewEvents.EVENTS| or getEvents().
GuestViewEvents.prototype.makeDomEvent = function(event, eventName) {
  var eventInfo =
      GuestViewEvents.EVENTS[eventName] || this.getEvents()[eventName];

  // Internal events are not dispatched as DOM events.
  if (eventInfo.internal) {
    return null;
  }

  var details = { bubbles: true };
  if (eventInfo.cancelable) {
    details.cancelable = true;
  }
  var domEvent = new Event(eventName, details);
  if (eventInfo.fields) {
    $Array.forEach(eventInfo.fields, function(field) {
      if (event[field] !== undefined) {
        domEvent[field] = event[field];
      }
    }.bind(this));
  }

  return domEvent;
};

// Adds an 'on<event>' property on the view, which can be used to set/unset
// an event handler.
GuestViewEvents.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  $Object.defineProperty(this.view.element, propertyName, {
    get: function() {
      return this.on[propertyName];
    }.bind(this),
    set: function(value) {
      if (this.on[propertyName]) {
        this.view.element.removeEventListener(eventName, this.on[propertyName]);
      }
      this.on[propertyName] = value;
      if (value) {
        this.view.element.addEventListener(eventName, value);
      }
    }.bind(this),
    enumerable: true
  });
};

// Implemented by the derived event manager, if one exists.
GuestViewEvents.prototype.getEvents = function() { return {}; };

// Exports.
exports.GuestViewEvents = GuestViewEvents;
exports.CreateEvent = CreateEvent;
