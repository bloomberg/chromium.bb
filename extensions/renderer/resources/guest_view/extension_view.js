// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the ExtensionView <extensionview>.

var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var ExtensionViewEvents = require('extensionViewEvents').ExtensionViewEvents;
var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;

function ExtensionViewImpl(extensionviewElement) {
  GuestViewContainer.call(this, extensionviewElement, 'extensionview');
  this.setupExtensionViewAttributes();

  new ExtensionViewEvents(this, this.viewInstanceId);
}

ExtensionViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

ExtensionViewImpl.VIEW_TYPE = 'ExtensionView';

ExtensionViewImpl.setupElement = function(proto) {
  var apiMethods = ExtensionViewImpl.getApiMethods();

  GuestViewContainer.forwardApiMethods(proto, apiMethods);
};

ExtensionViewImpl.prototype.createGuest = function() {
  this.guest.create(this.buildParams(), function() {
    this.attachWindow();
  }.bind(this));
};

ExtensionViewImpl.prototype.buildContainerParams = function() {
  var params = {};
  for (var i in this.attributes) {
    params[i] = this.attributes[i].getValue();
  }
  return params;
};

// This observer monitors mutations to attributes of the <extensionview>.
ExtensionViewImpl.prototype.handleAttributeMutation = function(
    attributeName, oldValue, newValue) {
  if (!this.attributes[attributeName])
    return;

  // Let the changed attribute handle its own mutation.
  this.attributes[attributeName].maybeHandleMutation(oldValue, newValue);
};

ExtensionViewImpl.prototype.onElementDetached = function() {
  this.guest.destroy();

  // Reset all attributes.
  for (var i in this.attributes) {
    this.attributes[i].reset();
  }
};

// Updates src upon loadcommit.
ExtensionViewImpl.prototype.onLoadCommit = function(url) {
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC].
      setValueIgnoreMutation(url);
};

GuestViewContainer.registerElement(ExtensionViewImpl);

// Exports.
exports.ExtensionViewImpl = ExtensionViewImpl;
