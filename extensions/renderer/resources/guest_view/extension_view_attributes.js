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
  this.extensionViewImpl.element.setAttribute(this.name, value || '');
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
ExtensionViewAttribute.prototype.handleMutation =
    function(oldValue, newValue) {};

// Attribute that handles the location and navigation of the extensionview.
function SrcAttribute(extensionViewImpl) {
  ExtensionViewAttribute.call(this, ExtensionViewConstants.ATTRIBUTE_SRC,
                              extensionViewImpl);
  this.setupMutationObserver();
  this.beforeFirstNavigation = true;
}

SrcAttribute.prototype.__proto__ = ExtensionViewAttribute.prototype;

SrcAttribute.prototype.setValueIgnoreMutation = function(value) {
  this.observer.takeRecords();
  this.ignoreMutation = true;
  this.extensionViewImpl.element.setAttribute(this.name, value || '');
  this.ignoreMutation = false;
}

SrcAttribute.prototype.handleMutation = function(oldValue, newValue) {
  if (this.attributes[attributeName].ignoreMutation)
    return;

  if (!newValue && oldValue) {
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  this.parse();
};

SrcAttribute.prototype.setupMutationObserver =
    function() {
  this.observer = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, function(mutation) {
      var oldValue = mutation.oldValue;
      var newValue = this.getValue();
      if (oldValue != newValue) {
        return;
      }
      this.handleMutation(oldValue, newValue);
    }.bind(this));
  }.bind(this));
  var params = {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: [this.name]
  };
  this.observer.observe(this.extensionViewImpl.element, params);
};

SrcAttribute.prototype.parse = function() {
  if (!this.extensionViewImpl.elementAttached || !this.getValue())
    return;

  if (!this.extensionViewImpl.guest.getId()) {
    if (this.beforeFirstNavigation) {
      this.beforeFirstNavigation = false;
      this.extensionViewImpl.createGuest();
    }
    return;
  }

  ExtensionViewInternal.navigate(this.extensionViewImpl.guest.getId(),
                                 this.getValue());
};

SrcAttribute.prototype.reset = function() {
  this.beforeFirstNavigation = true;
};

// -----------------------------------------------------------------------------

// Sets up all of the extensionview attributes.
ExtensionViewImpl.prototype.setupExtensionViewAttributes = function() {
  this.attributes = {};
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC] =
      new SrcAttribute(this);
};
