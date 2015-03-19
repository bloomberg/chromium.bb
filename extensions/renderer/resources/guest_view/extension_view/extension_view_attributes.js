// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the attributes of the <extensionview> tag.

var GuestViewAttributes = require('guestViewAttributes').GuestViewAttributes;
var ExtensionViewImpl = require('extensionView').ExtensionViewImpl;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;

// -----------------------------------------------------------------------------
// ExtensionAttribute object.

// Attribute that handles extension binded to the extensionview.
function ExtensionAttribute(view) {
  GuestViewAttributes.ReadOnlyAttribute.call(
      this, ExtensionViewConstants.ATTRIBUTE_EXTENSION, view);
}

ExtensionAttribute.prototype.__proto__ =
    GuestViewAttributes.ReadOnlyAttribute.prototype;

// -----------------------------------------------------------------------------
// SrcAttribute object.

// Attribute that handles the location and navigation of the extensionview.
function SrcAttribute(view) {
  GuestViewAttributes.Attribute.call(
      this, ExtensionViewConstants.ATTRIBUTE_SRC, view);
}

SrcAttribute.prototype.__proto__ = GuestViewAttributes.Attribute.prototype;

SrcAttribute.prototype.handleMutation = function(oldValue, newValue) {
  if (!newValue && oldValue) {
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  this.parse();
};

SrcAttribute.prototype.parse = function() {
  if (!this.view.elementAttached || !this.getValue() ||
      !this.view.guest.getId()) {
    return;
  }

  ExtensionViewInternal.navigate(this.view.guest.getId(), this.getValue());
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
