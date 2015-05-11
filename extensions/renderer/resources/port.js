// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Event = require('event_bindings').Event;
var messagingNatives = requireNative('messaging_natives');
var utils = require('utils');

// Port object.  Represents a connection to another script context through
// which messages can be passed.
function Port(portId, opt_name) {
  this.portId_ = portId;
  this.name = opt_name;

  var portSchema = {name: 'port', $ref: 'runtime.Port'};
  var messageSchema = {name: 'message', type: 'any', optional: true};
  var options = {unmanaged: true};

  this.onMessage = new Event(null, [messageSchema, portSchema], options);
  this.onDisconnect = new Event(null, [portSchema], options);
  this.onDestroy_ = null;
}

// Sends a message asynchronously to the context on the other end of this
// port.
Port.prototype.postMessage = function(msg) {
  // JSON.stringify doesn't support a root object which is undefined.
  if (msg === undefined)
    msg = null;
  msg = $JSON.stringify(msg);
  if (msg === undefined) {
    // JSON.stringify can fail with unserializable objects. Log an error and
    // drop the message.
    //
    // TODO(kalman/mpcomplete): it would be better to do the same validation
    // here that we do for runtime.sendMessage (and variants), i.e. throw an
    // schema validation Error, but just maintain the old behaviour until
    // there's a good reason not to (http://crbug.com/263077).
    console.error('Illegal argument to Port.postMessage');
    return;
  }
  messagingNatives.PostMessage(this.portId_, msg);
};

// Disconnects the port from the other end.
Port.prototype.disconnect = function() {
  messagingNatives.CloseChannel(this.portId_, true);
  this.destroy_();
};

Port.prototype.destroy_ = function() {
  if (this.onDestroy_)
    this.onDestroy_();
  // Destroy the onMessage/onDisconnect events in case the extension added
  // listeners, but didn't remove them, when the port closed.
  privates(this.onMessage).impl.destroy_();
  privates(this.onDisconnect).impl.destroy_();
  messagingNatives.PortRelease(this.portId_);
};

exports.Port = utils.expose('Port', Port, {
  functions: ['disconnect', 'postMessage'],
  properties: ['name', 'onMessage', 'onDisconnect']
});
