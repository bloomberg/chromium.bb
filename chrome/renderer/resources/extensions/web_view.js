// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

var watchForTag = require("tagWatcher").watchForTag;

var WEB_VIEW_ATTRIBUTES = ['name', 'src', 'partition'];

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
  'loadredirect' : ['oldUrl', 'newUrl', 'isTopLevel'],
  'loadstart' : ['url', 'isTopLevel'],
  'loadstop' : [],
  'sizechanged': ['oldHeight', 'oldWidth', 'newHeight', 'newWidth'],
};

window.addEventListener('DOMContentLoaded', function() {
  watchForTag('WEBVIEW', function(addedNode) { new WebView(addedNode); });
});

/**
 * @constructor
 */
function WebView(node) {
  this.node_ = node;
  var shadowRoot = node.webkitCreateShadowRoot();

  this.objectNode_ = document.createElement('object');
  this.objectNode_.type = 'application/browser-plugin';
  // The <object> node fills in the <browser> container.
  this.objectNode_.style.width = '100%';
  this.objectNode_.style.height = '100%';
  WEB_VIEW_ATTRIBUTES.forEach(function(attributeName) {
    this.objectNode_.setAttribute(
        attributeName, this.node_.getAttribute(attributeName));
  }, this);

  shadowRoot.appendChild(this.objectNode_);

  // this.objectNode_[apiMethod] are not necessarily defined immediately after
  // the shadow object is appended to the shadow root.
  var self = this;
  WEB_VIEW_API_METHODS.forEach(function(apiMethod) {
    node[apiMethod] = function(var_args) {
      return self.objectNode_[apiMethod].apply(self.objectNode_, arguments);
    };
  }, this);

  // Map attribute modifications on the <webview> tag to property changes in
  // the underlying <object> node.
  var handleMutation = this.handleMutation_.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(handleMutation);
  });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});

  var handleObjectMutation = this.handleObjectMutation_.bind(this);
  var objectObserver = new WebKitMutationObserver(function(mutations) {
    mutations.forEach(handleObjectMutation);
  });
  objectObserver.observe(
      this.objectNode_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});

  var objectNode = this.objectNode_;
  // Expose getters and setters for the attributes.
  WEB_VIEW_ATTRIBUTES.forEach(function(attributeName) {
    Object.defineProperty(this.node_, attributeName, {
      get: function() {
        return objectNode[attributeName];
      },
      set: function(value) {
        objectNode[attributeName] = value;
      },
      enumerable: true
    });
  }, this);

  // We cannot use {writable: true} property descriptor because we want dynamic
  // getter value.
  Object.defineProperty(this.node_, 'contentWindow', {
    get: function() {
      // TODO(fsamuel): This is a workaround to enable
      // contentWindow.postMessage until http://crbug.com/152006 is fixed.
      return objectNode.contentWindow.self;
    },
    // No setter.
    enumerable: true
  });

  for (var eventName in WEB_VIEW_EVENTS) {
    this.setupEvent_(eventName, WEB_VIEW_EVENTS[eventName]);
  }
}

/**
 * @private
 */
WebView.prototype.handleMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the <webview> and
  // updates the BrowserPlugin properties accordingly. In turn, updating
  // a BrowserPlugin property will update the corresponding BrowserPlugin
  // attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
  // details.
  this.objectNode_[mutation.attributeName] =
      this.node_.getAttribute(mutation.attributeName);
};

/**
 * @private
 */
WebView.prototype.handleObjectMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the BrowserPlugin and
  // updates the <webview> attributes accordingly.
  if (!this.objectNode_.hasAttribute(mutation.attributeName)) {
    // If an attribute is removed from the BrowserPlugin, then remove it
    // from the <webview> as well.
    this.node_.removeAttribute(mutation.attributeName);
  } else {
    // Update the <webview> attribute to match the BrowserPlugin attribute.
    this.node_.setAttribute(mutation.attributeName,
        this.objectNode_.getAttribute(mutation.attributeName));
  }
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
