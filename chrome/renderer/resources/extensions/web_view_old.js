// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

var WEB_VIEW_ATTRIBUTES = ['src', 'width', 'height'];

window.addEventListener('DOMContentLoaded', function() {
  // Handle <webview> tags already in the document.
  var webViewNodes = document.body.querySelectorAll('webview');
  for (var i = 0, webViewNode; webViewNode = webViewNodes[i]; i++) {
    new WebView(webViewNode);
  }

  // Handle <webview> tags added later.
  var documentObserver = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(function(mutation) {
      for (var i = 0, addedNode; addedNode = mutation.addedNodes[i]; i++) {
        if (addedNode.tagName == 'WEBVIEW') {
          new WebView(addedNode);
        }
      }
    });
  });
  documentObserver.observe(document, {subtree: true, childList: true});
});

/**
 * @constructor
 */
function WebView(node) {
  this.node_ = node;
  var shadowRoot = new WebKitShadowRoot(node);

  this.objectNode_ = document.createElement('object');
  this.objectNode_.type = 'application/browser-plugin';
  WEB_VIEW_ATTRIBUTES.forEach(this.copyAttribute_, this);
  shadowRoot.appendChild(this.objectNode_);

  // Map attribute modifications on the <webview> tag to changes on the
  // underlying <object> node.
  var handleMutation = this.handleMutation_.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(handleMutation);
  });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});

  // Expose getters and setters for the attributes.
  WEB_VIEW_ATTRIBUTES.forEach(function(attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        var value = node.getAttribute(attributeName);
        var numericValue = parseInt(value, 10);
        return isNaN(numericValue) ? value : numericValue;
      },
      set: function(value) {
        node.setAttribute(attributeName, value);
      },
      enumerable: true
    });
  }, this);
};

/**
 * @private
 */
WebView.prototype.handleMutation_ = function(mutation) {
  switch (mutation.attributeName) {
    case 'src':
      this.objectNode_.postMessage(this.node_.getAttribute('src'));
      break;
    default:
      this.copyAttribute_(mutation.attributeName);
      break;
  }
};

/**
 * @private
 */
WebView.prototype.copyAttribute_ = function(attributeName) {
  this.objectNode_.setAttribute(
      attributeName, this.node_.getAttribute(attributeName));
};
