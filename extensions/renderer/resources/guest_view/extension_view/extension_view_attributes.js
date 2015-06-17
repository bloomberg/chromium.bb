// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the attributes of the <extensionview> tag.

var GuestViewAttributes = require('guestViewAttributes').GuestViewAttributes;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;
var ExtensionViewImpl = require('extensionView').ExtensionViewImpl;
var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;

// -----------------------------------------------------------------------------
// ExtensionAttribute object.

// Attribute that handles the extension associated with the extensionview.
function ExtensionAttribute(view) {
  GuestViewAttributes.ReadOnlyAttribute.call(
      this, ExtensionViewConstants.ATTRIBUTE_EXTENSION, view);
}

ExtensionAttribute.prototype.__proto__ =
    GuestViewAttributes.ReadOnlyAttribute.prototype;

// -----------------------------------------------------------------------------
// SrcAttribute object.

// Attribute that handles the location and navigation of the extensionview.
// This is read only because we only want to be able to navigate to a src
// through the load API call, which checks for URL validity and the extension
// ID of the new src.
function SrcAttribute(view) {
  GuestViewAttributes.ReadOnlyAttribute.call(
      this, ExtensionViewConstants.ATTRIBUTE_SRC, view);
}

SrcAttribute.prototype.__proto__ =
    GuestViewAttributes.ReadOnlyAttribute.prototype;

// -----------------------------------------------------------------------------

// Sets up all of the extensionview attributes.
ExtensionViewImpl.prototype.setupAttributes = function() {
  this.attributes[ExtensionViewConstants.ATTRIBUTE_EXTENSION] =
      new ExtensionAttribute(this);
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC] =
      new SrcAttribute(this);
};
