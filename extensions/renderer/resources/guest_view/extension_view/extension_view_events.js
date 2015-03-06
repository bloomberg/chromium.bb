// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event management for ExtensionView.

var EventBindings = require('event_bindings');

var CreateEvent = function(name) {
  var eventOpts = {supportsListeners: true, supportsFilters: true};
  return new EventBindings.Event(name, undefined, eventOpts);
};

var EXTENSION_VIEW_EVENTS = {
  'loadcommit': {
    handler: function(event) {
      ExtensionViewEvents.prototype.handleLoadCommitEvent.call(this, event);
    },
    evt: CreateEvent('extensionViewInternal.onLoadCommit'),
  }
};

// Constructor.
function ExtensionViewEvents(extensionViewImpl, viewInstanceId) {
  this.extensionViewImpl = extensionViewImpl;
  this.viewInstanceId = viewInstanceId;

  // Set up the events.
  var events = this.getEvents();
  for (var eventName in events) {
    this.setupEvent(eventName, events[eventName]);
  }
}

ExtensionViewEvents.prototype.getEvents = function() {
  return EXTENSION_VIEW_EVENTS;
};

ExtensionViewEvents.prototype.setupEvent = function(name, info) {
  info.evt.addListener(function(e) {
    if (info.handler)
      info.handler.call(this, e);
  }.bind(this), {instanceId: this.viewInstanceId});
};

ExtensionViewEvents.prototype.handleLoadCommitEvent = function(event) {
  this.extensionViewImpl.onLoadCommit(event.url);
};

exports.ExtensionViewEvents = ExtensionViewEvents;
