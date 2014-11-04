// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the attributes of the <webview> tag.

var WebView = require('webView').WebView;
var WebViewConstants = require('webViewConstants').WebViewConstants;

// -----------------------------------------------------------------------------
// Attribute objects.

// Default implementation of a WebView attribute.
function WebViewAttribute(name, webViewImpl) {
  this.name = name;
  this.webViewImpl = webViewImpl;
  this.ignoreNextMutation = false;
}

// Retrieves and returns the attribute's value.
WebViewAttribute.prototype.getValue = function() {
  return this.webViewImpl.webviewNode.getAttribute(this.name) || '';
};

// Sets the attribute's value.
WebViewAttribute.prototype.setValue = function(value) {
  this.webViewImpl.webviewNode.setAttribute(this.name, value || '');
};

// Called when the attribute's value changes.
WebViewAttribute.prototype.handleMutation = function() {}

// Attribute specifying whether transparency is allowed in the webview.
function BooleanAttribute(name, webViewImpl) {
  WebViewAttribute.call(this, name, webViewImpl);
}

BooleanAttribute.prototype = new WebViewAttribute();

BooleanAttribute.prototype.getValue = function() {
  // This attribute is treated as a boolean, and is retrieved as such.
  return this.webViewImpl.webviewNode.hasAttribute(this.name);
}

BooleanAttribute.prototype.setValue = function(value) {
  if (!value) {
    this.webViewImpl.webviewNode.removeAttribute(this.name);
  } else {
    this.webViewImpl.webviewNode.setAttribute(this.name, '');
  }
}

// Attribute representing the state of the storage partition.
function Partition(webViewImpl) {
  WebViewAttribute.call(this,
                        WebViewConstants.ATTRIBUTE_PARTITION,
                        webViewImpl);
  this.validPartitionId = true;
}

Partition.prototype = new WebViewAttribute();

Partition.prototype.handleMutation = function(oldValue, newValue) {
  newValue = newValue || '';

  // The partition cannot change if the webview has already navigated.
  if (!this.webViewImpl.beforeFirstNavigation) {
    window.console.error(WebViewConstants.ERROR_MSG_ALREADY_NAVIGATED);
    this.ignoreNextMutation = true;
    this.webViewImpl.webviewNode.setAttribute(this.name, oldValue);
    return;
  }
  if (newValue == 'persist:') {
    this.validPartitionId = false;
    window.console.error(
        WebViewConstants.ERROR_MSG_INVALID_PARTITION_ATTRIBUTE);
  }
}

// -----------------------------------------------------------------------------

// Sets up all of the webview attributes.
WebView.prototype.setupWebViewAttributes = function() {
  this.attributes = {};

  // Initialize the attributes with special behavior (and custom attribute
  // objects).
  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY] =
      new BooleanAttribute(WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY, this);
  this.attributes[WebViewConstants.ATTRIBUTE_AUTOSIZE] =
      new BooleanAttribute(WebViewConstants.ATTRIBUTE_AUTOSIZE, this);
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION] = new Partition(this);

  // Initialize the remaining attributes, which have default behavior.
  var defaultAttributes = [WebViewConstants.ATTRIBUTE_MAXHEIGHT,
                           WebViewConstants.ATTRIBUTE_MAXWIDTH,
                           WebViewConstants.ATTRIBUTE_MINHEIGHT,
                           WebViewConstants.ATTRIBUTE_MINWIDTH,
                           WebViewConstants.ATTRIBUTE_NAME,
                           WebViewConstants.ATTRIBUTE_SRC];
  for (var i = 0; defaultAttributes[i]; ++i) {
    this.attributes[defaultAttributes[i]] =
        new WebViewAttribute(defaultAttributes[i], this);
  }
};
