// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements a wrapper for a guestview that manages its
// creation, attaching, and destruction.

var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var GuestViewInternalNatives = requireNative('guest_view_internal');

// Possible states.
var GUEST_STATE_ATTACHED = 'attached';
var GUEST_STATE_CREATED = 'created';
var GUEST_STATE_DESTROYED = 'destroyed';
var GUEST_STATE_START = 'start';

// Error messages.
var ERROR_MSG_ATTACH = 'Error calling attach: ';
var ERROR_MSG_CREATE = 'Error calling create: ';
var ERROR_MSG_DESTROY = 'Error calling destroy: ';
var ERROR_MSG_ALREADY_CREATED = 'The guest has already been created.';
var ERROR_MSG_DESTROYED = 'The guest is destroyed.';
var ERROR_MSG_INVALID_STATE = 'The guest is in an invalid state.';
var ERROR_MSG_NOT_CREATED = 'The guest has not been created.';

// Contains and hides the internal implementation details of |GuestView|,
// including maintaining its state and enforcing the proper usage of its API
// fucntions.
function GuestViewImpl(viewType) {
  this.contentWindow = null;
  this.actionQueue = [];
  this.pendingAction = null;
  this.id = 0;
  this.state = GUEST_STATE_START;
  this.viewType = viewType;
}

// Callback wrapper that is used to call the callback of the pending action (if
// one exists), and then performs the next action in the queue.
GuestViewImpl.prototype.handleCallback = function(callback) {
  if (callback) {
    callback();
  }
  this.pendingAction = null;
  this.performNextAction();
}

// Perform the next action in the queue, if one exists.
GuestViewImpl.prototype.performNextAction = function() {
  // Make sure that there is not already an action in progress, and that there
  // exists a queued action to perform.
  if (!this.pendingAction && this.actionQueue.length) {
    this.pendingAction = this.actionQueue.shift();
    this.pendingAction();
  }
};

// Internal implementation of attach().
GuestViewImpl.prototype.attachImpl = function(
    internalInstanceId, attachParams, callback) {
  // Check the current state.
  switch (this.state) {
    case GUEST_STATE_START:
      window.console.error(ERROR_MSG_ATTACH + ERROR_MSG_NOT_CREATED);
      return;
    case GUEST_STATE_DESTROYED:
      window.console.error(ERROR_MSG_ATTACH + ERROR_MSG_DESTROYED);
      return;
    case GUEST_STATE_ATTACHED:
    case GUEST_STATE_CREATED:
      break;
    default:
      window.console.error(ERROR_MSG_ATTACH + ERROR_MSG_INVALID_STATE);
      return;
  }

  // Callback wrapper function to store the contentWindow from the attachGuest()
  // callback, and advance the queue.
  var storeContentWindow = function(callback, contentWindow) {
    this.contentWindow = contentWindow;
    this.handleCallback(callback);
  }

  GuestViewInternalNatives.AttachGuest(internalInstanceId,
                                       this.id,
                                       attachParams,
                                       storeContentWindow.bind(this, callback));

  this.state = GUEST_STATE_ATTACHED;
};

// Internal implementation of create().
GuestViewImpl.prototype.createImpl = function(createParams, callback) {
  // Check the current state.
  switch (this.state) {
    case GUEST_STATE_DESTROYED:
      window.console.error(ERROR_MSG_CREATE + ERROR_MSG_DESTROYED);
      return;
    case GUEST_STATE_ATTACHED:
    case GUEST_STATE_CREATED:
      window.console.error(ERROR_MSG_CREATE + ERROR_MSG_ALREADY_CREATED);
      return;
    case GUEST_STATE_START:
      break;
    default:
      window.console.error(ERROR_MSG_CREATE + ERROR_MSG_INVALID_STATE);
      return;
  }

  // Callback wrapper function to store the guestInstanceId from the
  // createGuest() callback, and advance the queue.
  var storeId = function(callback, guestInstanceId) {
    this.id = guestInstanceId;
    this.handleCallback(callback);
  }

  GuestViewInternal.createGuest(this.viewType,
                                createParams,
                                storeId.bind(this, callback));

  this.state = GUEST_STATE_CREATED;
};

// Internal implementation of destroy().
GuestViewImpl.prototype.destroyImpl = function() {
  // Check the current state.
  switch (this.state) {
    case GUEST_STATE_DESTROYED:
      window.console.error(ERROR_MSG_DESTROY + ERROR_MSG_DESTROYED);
      return;
    case GUEST_STATE_START:
      window.console.error(ERROR_MSG_DESTROY + ERROR_MSG_NOT_CREATED);
      return;
    case GUEST_STATE_ATTACHED:
    case GUEST_STATE_CREATED:
      break;
    default:
      window.console.error(ERROR_MSG_DESTROY + ERROR_MSG_INVALID_STATE);
      return;
  }

  GuestViewInternal.destroyGuest(this.id);
  this.contentWindow = null;
  this.id = 0;
  this.handleCallback();

  this.state = GUEST_STATE_DESTROYED;
};

// The exposed interface to a guestview. Exposes in its API the functions
// attach(), create(), destroy(), and getId(). All other implementation details
// are hidden.
function GuestView(viewType) {
  privates(this).internal = new GuestViewImpl(viewType);
}

// Attaches the guestview to the container with ID |internalInstanceId|.
GuestView.prototype.attach = function(
    internalInstanceId, attachParams, callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.attachImpl.bind(
      internal, internalInstanceId, attachParams, callback));
  internal.performNextAction();
};

// Creates the guestview.
GuestView.prototype.create = function(createParams, callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.createImpl.bind(
      internal, createParams, callback));
  internal.performNextAction();
};

// Destroys the guestview. Nothing can be done with the guestview after it has
// been destroyed.
GuestView.prototype.destroy = function() {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.destroyImpl.bind(internal));
  internal.performNextAction();
};

// Returns the contentWindow for this guestview.
GuestView.prototype.getContentWindow = function() {
  var internal = privates(this).internal;
  return internal.contentWindow;
};

// Returns the ID for this guestview.
GuestView.prototype.getId = function() {
  var internal = privates(this).internal;
  return internal.id;
};

// Exports
exports.GuestView = GuestView;
