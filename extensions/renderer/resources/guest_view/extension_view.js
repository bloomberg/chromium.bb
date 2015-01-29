// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the ExtensionView <extensionview>.

var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;

function ExtensionViewImpl(extensionviewElement) {
  GuestViewContainer.call(this, extensionviewElement, 'extensionview');
  this.setupExtensionViewAttributes();
}

ExtensionViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

ExtensionViewImpl.VIEW_TYPE = 'ExtensionView';

ExtensionViewImpl.setupElement = function(proto) {
  var apiMethods = [
    'navigate'
  ];

  GuestViewContainer.forwardApiMethods(proto, apiMethods);
};

ExtensionViewImpl.prototype.createGuest = function() {
  this.guest.create(this.buildParams(), function() {
    this.attachWindow();
  }.bind(this));
};

ExtensionViewImpl.prototype.onElementAttached = function() {
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC].parse();
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

  // Let the changed attribute handle its own mutation;
  this.attributes[attributeName].handleMutation(oldValue, newValue);
};

ExtensionViewImpl.prototype.onElementDetached = function() {
  this.guest.destroy();
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC].reset();
};

GuestViewContainer.registerElement(ExtensionViewImpl);

// Exports.
exports.ExtensionViewImpl = ExtensionViewImpl;