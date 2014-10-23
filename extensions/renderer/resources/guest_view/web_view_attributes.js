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
  this.value = '';
  this.webViewImpl = webViewImpl;
}

WebViewAttribute.prototype.getValue = function() {
  return this.value || '';
};

WebViewAttribute.prototype.setValue = function(value) {
  this.value = value;
};

// Attribute representing the state of the storage partition.
function Partition(webViewImpl) {
  this.validPartitionId = true;
  this.persistStorage = false;
  this.storagePartitionId = '';
  this.webViewImpl = webViewImpl;
}

Partition.prototype = new WebViewAttribute(
    WebViewConstants.ATTRIBUTE_PARTITION);

Partition.prototype.getValue = function() {
  if (!this.validPartitionId) {
    return '';
  }
  return (this.persistStorage ? 'persist:' : '') + this.storagePartitionId;
};

Partition.prototype.setValue = function(value) {
  var result = {};
  var hasNavigated = !this.webViewImpl.beforeFirstNavigation;
  if (hasNavigated) {
    result.error = WebViewConstants.ERROR_MSG_ALREADY_NAVIGATED;
    return result;
  }
  if (!value) {
    value = '';
  }

  var LEN = 'persist:'.length;
  if (value.substr(0, LEN) == 'persist:') {
    value = value.substr(LEN);
    if (!value) {
      this.validPartitionId = false;
      result.error = WebViewConstants.ERROR_MSG_INVALID_PARTITION_ATTRIBUTE;
      return result;
    }
    this.persistStorage = true;
  } else {
    this.persistStorage = false;
  }

  this.storagePartitionId = value;
  return result;
};

// -----------------------------------------------------------------------------

// Sets up all of the webview attributes.
WebView.prototype.setupWebViewAttributes = function() {
  this.attributes = {};

  // Initialize the attributes with special behavior (and custom attribute
  // objects).
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION] = new Partition(this);

  // Initialize the remaining attributes, which have default behavior.
  var defaultAttributes = [WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY,
                           WebViewConstants.ATTRIBUTE_AUTOSIZE,
                           WebViewConstants.ATTRIBUTE_MAXHEIGHT,
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
