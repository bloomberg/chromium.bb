// Copyright 2014 The Chromium Authors. All rights reserved.
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
var GUEST_STATE_START = 'start';

// Error messages.
var ERROR_MSG_ATTACH = 'Error calling attach: ';
var ERROR_MSG_CREATE = 'Error calling create: ';
var ERROR_MSG_DESTROY = 'Error calling destroy: ';
var ERROR_MSG_DETACH = 'Error calling detach: ';
var ERROR_MSG_SETAUTOSIZE = 'Error calling setAutoSize: ';
var ERROR_MSG_ALREADY_ATTACHED = 'The guest has already been attached.';
var ERROR_MSG_ALREADY_CREATED = 'The guest has already been created.';
var ERROR_MSG_INVALID_STATE = 'The guest is in an invalid state.';
var ERROR_MSG_NOT_ATTACHED = 'The guest is not attached.';
var ERROR_MSG_NOT_CREATED = 'The guest has not been created.';

// Contains and hides the internal implementation details of |GuestView|,
// including maintaining its state and enforcing the proper usage of its API
// fucntions.

function GuestViewImpl(viewType, guestInstanceId) {
  if (guestInstanceId) {
    this.id = guestInstanceId;
    this.state = GUEST_STATE_CREATED;
  } else {
    this.id = 0;
    this.state = GUEST_STATE_START;
  }
  this.actionQueue = [];
  this.contentWindow = null;
  this.pendingAction = null;
  this.viewType = viewType;
  this.internalInstanceId = 0;
}

// Callback wrapper that is used to call the callback of the pending action (if
// one exists), and then performs the next action in the queue.
GuestViewImpl.prototype.handleCallback = function(callback) {
  if (callback) {
    callback();
  }
  this.pendingAction = null;
  this.performNextAction();
};

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
    case GUEST_STATE_ATTACHED:
      window.console.error(ERROR_MSG_ATTACH + ERROR_MSG_ALREADY_ATTACHED);
      return;
    case GUEST_STATE_CREATED:
      break;
    default:
      window.console.error(ERROR_MSG_ATTACH + ERROR_MSG_INVALID_STATE);
      return;
  }

  // Callback wrapper function to store the contentWindow from the attachGuest()
  // callback, handle potential attaching failure, register an automatic detach,
  // and advance the queue.
  var callbackWrapper = function(callback, contentWindow) {
    this.contentWindow = contentWindow;

    // Check if attaching failed.
    if (this.contentWindow === null) {
      this.state = GUEST_STATE_CREATED;
    } else {
      // Detach automatically when the container is destroyed.
      GuestViewInternalNatives.RegisterDestructionCallback(internalInstanceId,
                                                           function() {
        if (this.state == GUEST_STATE_ATTACHED) {
          this.contentWindow = null;
          this.internalInstanceId = 0;
          this.state = GUEST_STATE_CREATED;
        }
      }.bind(this));
    }

    this.handleCallback(callback);
  };

  GuestViewInternalNatives.AttachGuest(internalInstanceId,
                                       this.id,
                                       attachParams,
                                       callbackWrapper.bind(this, callback));

  this.internalInstanceId = internalInstanceId;
  this.state = GUEST_STATE_ATTACHED;
};

// Internal implementation of create().
GuestViewImpl.prototype.createImpl = function(createParams, callback) {
  // Check the current state.
  switch (this.state) {
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
  // createGuest() callback, handle potential creation failure, and advance the
  // queue.
  var callbackWrapper = function(callback, guestInstanceId) {
    this.id = guestInstanceId;

    // Check if creation failed.
    if (this.id === 0) {
      this.state = GUEST_STATE_START;
    }

    this.handleCallback(callback);
  };

  GuestViewInternal.createGuest(this.viewType,
                                createParams,
                                callbackWrapper.bind(this, callback));

  this.state = GUEST_STATE_CREATED;
};

// Internal implementation of destroy().
GuestViewImpl.prototype.destroyImpl = function(callback) {
  // Check the current state.
  switch (this.state) {
    case GUEST_STATE_ATTACHED:
    case GUEST_STATE_CREATED:
      break;
    case GUEST_STATE_START:
      return;
    default:
      window.console.error(ERROR_MSG_DESTROY + ERROR_MSG_INVALID_STATE);
      return;
  }

  GuestViewInternal.destroyGuest(this.id,
                                 this.handleCallback.bind(this, callback));

  this.contentWindow = null;
  this.id = 0;
  this.internalInstanceId = 0;
  this.state = GUEST_STATE_START;
};

// Internal implementation of detach().
GuestViewImpl.prototype.detachImpl = function(callback) {
  // Check the current state.
  switch (this.state) {
    case GUEST_STATE_ATTACHED:
      break;
    case GUEST_STATE_CREATED:
    case GUEST_STATE_START:
      window.console.error(ERROR_MSG_DETACH + ERROR_MSG_NOT_ATTACHED);
      return;
    default:
      window.console.error(ERROR_MSG_DETACH + ERROR_MSG_INVALID_STATE);
      return;
  }

  GuestViewInternalNatives.DetachGuest(
      this.internalInstanceId,
      this.handleCallback.bind(this, callback));

  this.contentWindow = null;
  this.internalInstanceId = 0;
  this.state = GUEST_STATE_CREATED;
};

// Internal implementation of setAutoSize().
GuestViewImpl.prototype.setAutoSizeImpl = function(autoSizeParams, callback) {
  // Check the current state.
  switch (this.state) {
    case GUEST_STATE_ATTACHED:
    case GUEST_STATE_CREATED:
      break;
    case GUEST_STATE_START:
      window.console.error(ERROR_MSG_SETAUTOSIZE + ERROR_MSG_NOT_CREATED);
      return;
    default:
      window.console.error(ERROR_MSG_SETAUTOSIZE + ERROR_MSG_INVALID_STATE);
      return;
  }

  GuestViewInternal.setAutoSize(this.id, autoSizeParams,
                                this.handleCallback.bind(this, callback));
};

// The exposed interface to a guestview. Exposes in its API the functions
// attach(), create(), destroy(), and getId(). All other implementation details
// are hidden.
function GuestView(viewType, guestInstanceId) {
  privates(this).internal = new GuestViewImpl(viewType, guestInstanceId);
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
GuestView.prototype.destroy = function(callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.destroyImpl.bind(internal, callback));
  internal.performNextAction();
};

// Detaches the guestview from its container.
// Note: This is not currently used.
GuestView.prototype.detach = function(callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.detachImpl.bind(internal, callback));
  internal.performNextAction();
};

// Adjusts the guestview's sizing parameters.
GuestView.prototype.setAutoSize = function(autoSizeParams, callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.setAutoSizeImpl.bind(
      internal, autoSizeParams, callback));
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
