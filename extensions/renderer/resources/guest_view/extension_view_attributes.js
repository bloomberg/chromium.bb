// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the attributes of the <extensionview> tag.

var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var ExtensionViewImpl = require('extensionView').ExtensionViewImpl;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;

// -----------------------------------------------------------------------------
// Attribute objects.

// Default implementation of a ExtensionView attribute.
function ExtensionViewAttribute(name, extensionViewImpl) {
  this.name = name;
  this.extensionViewImpl = extensionViewImpl;
  this.ignoreMutation = false;

  this.defineProperty();
}

// Retrieves and returns the attribute's value.
ExtensionViewAttribute.prototype.getValue = function() {
  return this.extensionViewImpl.element.getAttribute(this.name) || '';
};

// Sets the attribute's value.
ExtensionViewAttribute.prototype.setValue = function(value) {
  this.extensionViewImpl.element.setAttribute(this.name, value || '');
};

// Changes the attribute's value without triggering its mutation handler.
ExtensionViewAttribute.prototype.setValueIgnoreMutation = function(value) {
  this.ignoreMutation = true;
  this.setValue(value);
  this.ignoreMutation = false;
}

// Defines this attribute as a property on the extensionview node.
ExtensionViewAttribute.prototype.defineProperty = function() {
  Object.defineProperty(this.extensionViewImpl.element, this.name, {
    get: function() {
      return this.getValue();
    }.bind(this),
    set: function(value) {
      this.setValue(value);
    }.bind(this),
    enumerable: true
  });
};

// Called when the attribute's value changes.
ExtensionViewAttribute.prototype.maybeHandleMutation =
    function(oldValue, newValue) {
  if (this.ignoreMutation)
    return;

  this.handleMutation(oldValue, newValue);
}

// Called when a change that isn't ignored occurs to the attribute's value.
ExtensionViewAttribute.prototype.handleMutation =
    function(oldValue, newValue) {};

ExtensionViewAttribute.prototype.reset = function() {
  this.setValueIgnoreMutation();
}

// Attribute that handles extension binded to the extensionview.
function ExtensionAttribute(extensionViewImpl) {
  ExtensionViewAttribute.call(this, ExtensionViewConstants.ATTRIBUTE_EXTENSION,
                              extensionViewImpl);
}

ExtensionAttribute.prototype.__proto__ = ExtensionViewAttribute.prototype;

ExtensionAttribute.prototype.handleMutation = function(oldValue, newValue) {
  this.setValueIgnoreMutation(oldValue);
}

// Attribute that handles the location and navigation of the extensionview.
function SrcAttribute(extensionViewImpl) {
  ExtensionViewAttribute.call(this, ExtensionViewConstants.ATTRIBUTE_SRC,
                              extensionViewImpl);
}

SrcAttribute.prototype.__proto__ = ExtensionViewAttribute.prototype;

SrcAttribute.prototype.handleMutation = function(oldValue, newValue) {
  if (!newValue && oldValue) {
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  this.parse();
};

SrcAttribute.prototype.parse = function() {
  if (!this.extensionViewImpl.elementAttached || !this.getValue())
    return;

  if (!this.extensionViewImpl.guest.getId())
    return;

  ExtensionViewInternal.navigate(this.extensionViewImpl.guest.getId(),
                                 this.getValue());
};

// -----------------------------------------------------------------------------

// Sets up all of the extensionview attributes.
ExtensionViewImpl.prototype.setupExtensionViewAttributes = function() {
  this.attributes = {};
  this.attributes[ExtensionViewConstants.ATTRIBUTE_EXTENSION] =
      new ExtensionAttribute(this);
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC] =
      new SrcAttribute(this);
};
