// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the public-facing API functions for the
// <extensionview> tag.

var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;
var ExtensionViewImpl = require('extensionView').ExtensionViewImpl;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;

// An array of <extensionview>'s public-facing API methods.
var EXTENSION_VIEW_API_METHODS = [
  // Sets the extension ID and src for the guest. Must be called every time
  // the extension ID changes. This is the only way to set the extension ID.
  'connect'
];

// -----------------------------------------------------------------------------
// Custom API method implementations.

ExtensionViewImpl.prototype.connect = function(extension, src) {
  this.guest.destroy();

  // Sets the extension id and src.
  this.attributes[ExtensionViewConstants.ATTRIBUTE_EXTENSION]
      .setValueIgnoreMutation(extension);
  this.attributes[ExtensionViewConstants.ATTRIBUTE_SRC]
      .setValueIgnoreMutation(src);

  this.createGuest();
};

// -----------------------------------------------------------------------------

ExtensionViewImpl.getApiMethods = function() {
  return EXTENSION_VIEW_API_METHODS;
};
