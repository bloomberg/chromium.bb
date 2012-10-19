// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <browser> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

var BROWSER_TAG_ATTRIBUTES = ['src', 'width', 'height'];

var BROWSER_TAG_READONLY_ATTRIBUTES = ['contentWindow'];

// All exposed api methods for <browser>, these are forwarded to the browser
// plugin.
var BROWSER_TAG_API_METHODS = [
    'addEventListener',
    'back',
    'canGoBack',
    'canGoForward',
    'forward',
    'getProcessId',
    'go',
    'reload',
    'removeEventListener',
    'stop',
    'terminate'
    ];

window.addEventListener('DOMContentLoaded', function() {
  // Handle <browser> tags already in the document.
  var browserNodes = document.body.querySelectorAll('browser');
  for (var i = 0, browserNode; browserNode = browserNodes[i]; i++) {
    new BrowserTag(browserNode);
  }

  // Handle <browser> tags added later.
  var documentObserver = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(function(mutation) {
      for (var i = 0, addedNode; addedNode = mutation.addedNodes[i]; i++) {
        if (addedNode.tagName == 'BROWSER') {
          new BrowserTag(addedNode);
        }
      }
    });
  });
  documentObserver.observe(document, {subtree: true, childList: true});
});

/**
 * @constructor
 */
function BrowserTag(node) {
  this.node_ = node;
  var shadowRoot = new WebKitShadowRoot(node);

  this.objectNode_ = document.createElement('object');
  this.objectNode_.type = 'application/browser-plugin';
  BROWSER_TAG_ATTRIBUTES.forEach(this.copyAttribute_, this);

  shadowRoot.appendChild(this.objectNode_);

  // this.objectNode_[apiMethod] are defined after the shadow object is appended
  // to the shadow root.
  BROWSER_TAG_API_METHODS.forEach(function(apiMethod) {
    node[apiMethod] = this.objectNode_[apiMethod].bind(this.objectNode_);
  }, this);

  // Map attribute modifications on the <browser> tag to changes on the
  // underlying <object> node.
  var handleMutation = this.handleMutation_.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(handleMutation);
  });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: BROWSER_TAG_ATTRIBUTES});

  var objectNode = this.objectNode_;
  // Expose getters and setters for the attributes.
  BROWSER_TAG_ATTRIBUTES.forEach(function(attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        if (attributeName == 'src') {
          // Always read src attribute from the plugin <object> since: a) It can
          // have different value when empty src is set. b) BrowserPlugin
          // updates its src attribute on guest-initiated navigations.
          return objectNode.src;
        }
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

  // We cannot use {writable: true} property descriptor because we want dynamic
  // getter value.
  BROWSER_TAG_READONLY_ATTRIBUTES.forEach(function(attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        // Read these attributes from the plugin <object>.
        return objectNode[attributeName];
      },
      // No setter.
      enumerable: true
    })
  }, this);
};

/**
 * @private
 */
BrowserTag.prototype.handleMutation_ = function(mutation) {
  switch (mutation.attributeName) {
    case 'src':
      // We need to set .src directly on the shadow element so that
      // BrowserPluginBindings catches this as src attribute mutation. The
      // bindings would catch 'SetAttribute' method call with src as argument
      // otherwise.
      this.objectNode_.src = this.node_.getAttribute('src');
      break;
    default:
      this.copyAttribute_(mutation.attributeName);
      break;
  }
};

/**
 * @private
 */
BrowserTag.prototype.copyAttribute_ = function(attributeName) {
  this.objectNode_.setAttribute(
      attributeName, this.node_.getAttribute(attributeName));
};
