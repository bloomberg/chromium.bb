// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

var WEB_VIEW_ATTRIBUTES = ['src'];

var WEB_VIEW_READONLY_ATTRIBUTES = ['contentWindow'];

// All exposed api methods for <webview>, these are forwarded to the browser
// plugin.
var WEB_VIEW_API_METHODS = [
  'back',
  'canGoBack',
  'canGoForward',
  'forward',
  'getProcessId',
  'go',
  'reload',
  'stop',
  'terminate'
];

var WEB_VIEW_EVENTS = {
  'exit' : ['processId', 'reason'],
  'loadabort' : ['url', 'isTopLevel', 'reason'],
  'loadcommit' : ['url', 'isTopLevel'],
  'loadredirect' : ['oldurl', 'newurl', 'isTopLevel'],
  'loadstart' : ['url', 'isTopLevel'],
  'loadstop' : [],
  'sizechanged': ['oldHeight', 'oldWidth', 'newHeight', 'newWidth'],
};

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
  // The <object> node fills in the <browser> container.
  this.objectNode_.style.width = '100%';
  this.objectNode_.style.height = '100%';
  WEB_VIEW_ATTRIBUTES.forEach(this.copyAttribute_, this);

  shadowRoot.appendChild(this.objectNode_);

  // this.objectNode_[apiMethod] are defined after the shadow object is appended
  // to the shadow root.
  WEB_VIEW_API_METHODS.forEach(function(apiMethod) {
    node[apiMethod] = this.objectNode_[apiMethod].bind(this.objectNode_);
  }, this);

  // Map attribute modifications on the <webview> tag to changes on the
  // underlying <object> node.
  var handleMutation = this.handleMutation_.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(handleMutation);
  });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});

  var objectNode = this.objectNode_;
  // Expose getters and setters for the attributes.
  WEB_VIEW_ATTRIBUTES.forEach(function(attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        if (attributeName == 'src') {
          // Always read src attribute from the plugin <object> since: a) It can
          // have different value when empty src is set. b) BrowserPlugin
          // updates its src attribute on guest-initiated navigations.
          return objectNode.src;
        }
        return node.getAttribute(attributeName);
      },
      set: function(value) {
        node.setAttribute(attributeName, value);
      },
      enumerable: true
    });
  }, this);

  // We cannot use {writable: true} property descriptor because we want dynamic
  // getter value.
  WEB_VIEW_READONLY_ATTRIBUTES.forEach(function(attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        // Read these attributes from the plugin <object>.
        return objectNode[attributeName];
      },
      // No setter.
      enumerable: true
    })
  }, this);

  for (var eventName in WEB_VIEW_EVENTS) {
    this.setupEvent_(eventName, WEB_VIEW_EVENTS[eventName]);
  }
};

/**
 * @private
 */
WebView.prototype.handleMutation_ = function(mutation) {
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
WebView.prototype.copyAttribute_ = function(attributeName) {
  this.objectNode_.setAttribute(
      attributeName, this.node_.getAttribute(attributeName));
};

/**
 * @private
 */
WebView.prototype.setupEvent_ = function(eventname, attribs) {
  var node = this.node_;
  this.objectNode_.addEventListener('-internal-' + eventname, function(e) {
    var evt = new Event(eventname);
    var detail = e.detail ? JSON.parse(e.detail) : {};
    attribs.forEach(function(attribName) {
      evt[attribName] = detail[attribName];
    });
    node.dispatchEvent(evt);
  });
}
