// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EventBindings = require('event_bindings');

var CreateEvent = function(name) {
  var eventOpts = {supportsListeners: true, supportsFilters: true};
  return new EventBindings.Event(name, undefined, eventOpts);
};

var EXTENSION_OPTIONS_EVENTS = {
  'load': {
    evt: CreateEvent('extensionOptionsInternal.onLoad'),
    fields: []
  },
}

/**
 * @constructor
 */
function ExtensionOptionsEvents(extensionOptionsInternal, viewInstanceId) {
  this.extensionOptionsInternal = extensionOptionsInternal;
  this.viewInstanceId = viewInstanceId;
  this.setup();
}

// Sets up events.
ExtensionOptionsEvents.prototype.setup = function() {
  for (var eventName in EXTENSION_OPTIONS_EVENTS) {
    this.setupEvent(eventName, EXTENSION_OPTIONS_EVENTS[eventName]);
  }
};

ExtensionOptionsEvents.prototype.setupEvent = function(name, info) {
  var self = this;
  info.evt.addListener(function(e) {
    var details = {bubbles:true};
    if (info.cancelable)
      details.cancelable = true;
    var extensionOptionsEvent = new Event(name, details);
    $Array.forEach(info.fields, function(field) {
      if (e.hasOwnProperty(field)) {
        extensionOptionsEvent[field] = e[field];
      }
    });
    if (info.customHandler) {
      info.customHandler(self, e, extensionOptionsEvent);
      return;
    }
    self.extensionOptionsInternal.dispatchEvent(extensionOptionsEvent);
  }, {instanceId: self.viewInstanceId});

  this.extensionOptionsInternal.setupEventProperty(name);
};

exports.ExtensionOptionsEvents = ExtensionOptionsEvents;
exports.CreateEvent = CreateEvent;
