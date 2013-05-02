// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

var chrome = requireNative('chrome').GetChrome();
var forEach = require('utils').forEach;
var watchForTag = require('tagWatcher').watchForTag;

var WEB_VIEW_ATTRIBUTES = ['name', 'src', 'partition', 'autosize', 'minheight',
    'minwidth', 'maxheight', 'maxwidth'];

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
  'close': [],
  'consolemessage': ['level', 'message', 'line', 'sourceId'],
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
  // The <object> node fills in the <webview> container.
  this.objectNode_.style.width = '100%';
  this.objectNode_.style.height = '100%';
  forEach(WEB_VIEW_ATTRIBUTES, function(i, attributeName) {
    // Only copy attributes that have been assigned values, rather than copying
    // a series of undefined attributes to BrowserPlugin.
    if (this.node_.hasAttribute(attributeName)) {
      this.objectNode_.setAttribute(
          attributeName, this.node_.getAttribute(attributeName));
    }
  }, this);

  if (!this.node_.hasAttribute('tabIndex')) {
    // <webview> needs a tabIndex in order to respond to keyboard focus.
    // TODO(fsamuel): This introduces unexpected tab ordering. We need to find
    // a way to take keyboard focus without messing with tab ordering.
    // See http://crbug.com/231664.
    this.node_.setAttribute('tabIndex', 0);
  }
  var self = this;
  this.node_.addEventListener('focus', function(e) {
    // Focus the BrowserPlugin when the <webview> takes focus.
    self.objectNode_.focus();
  });
  this.node_.addEventListener('blur', function(e) {
    // Blur the BrowserPlugin when the <webview> loses focus.
    self.objectNode_.blur();
  });

  shadowRoot.appendChild(this.objectNode_);

  // this.objectNode_[apiMethod] are not necessarily defined immediately after
  // the shadow object is appended to the shadow root.
  forEach(WEB_VIEW_API_METHODS, function(i, apiMethod) {
    node[apiMethod] = function(var_args) {
      return self.objectNode_[apiMethod].apply(self.objectNode_, arguments);
    };
  }, this);

  // Map attribute modifications on the <webview> tag to property changes in
  // the underlying <object> node.
  var handleMutation = function(i, mutation) {
    this.handleMutation_(mutation);
  }.bind(this);
  var observer = new WebKitMutationObserver(function(mutations) {
    forEach(mutations, handleMutation);
  });
  observer.observe(
      this.node_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});

  var handleObjectMutation = function(i, mutation) {
    this.handleObjectMutation_(mutation);
  }.bind(this);
  var objectObserver = new WebKitMutationObserver(function(mutations) {
    forEach(mutations, handleObjectMutation);
  });
  objectObserver.observe(
      this.objectNode_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});

  var objectNode = this.objectNode_;
  // Expose getters and setters for the attributes.
  forEach(WEB_VIEW_ATTRIBUTES, function(i, attributeName) {
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
      if (objectNode.contentWindow)
        return objectNode.contentWindow.self;
      console.error('contentWindow is not available at this time. ' +
          'It will become available when the page has finished loading.');
    },
    // No setter.
    enumerable: true
  });

  for (var eventName in WEB_VIEW_EVENTS) {
    this.setupEvent_(eventName, WEB_VIEW_EVENTS[eventName]);
  }
  this.maybeSetupNewWindowEvent_();
  this.maybeSetupPermissionEvent_();
  this.maybeSetupExecuteScript_();
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
    // Note: Calling setAttribute on <webview> will trigger its mutation
    // observer which will then propagate that attribute to BrowserPlugin. In
    // cases where we permit assigning a BrowserPlugin attribute the same value
    // again (such as navigation when crashed), this could end up in an infinite
    // loop. Thus, we avoid this loop by only updating the <webview> attribute
    // if the BrowserPlugin attributes differs from it.
    var oldValue = this.node_.getAttribute(mutation.attributeName);
    var newValue = this.objectNode_.getAttribute(mutation.attributeName);
    if (newValue != oldValue) {
      this.node_.setAttribute(mutation.attributeName, newValue);
    }
  }
};

/**
 * @private
 */
WebView.prototype.setupEvent_ = function(eventname, attribs) {
  var node = this.node_;
  this.objectNode_.addEventListener('-internal-' + eventname, function(e) {
    var evt = new Event(eventname, { bubbles: true });
    var detail = e.detail ? JSON.parse(e.detail) : {};
    forEach(attribs, function(i, attribName) {
      evt[attribName] = detail[attribName];
    });
    node.dispatchEvent(evt);
  });
};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebView.prototype.maybeSetupNewWindowEvent_ = function() {};

/**
 * Implemented when experimental permission is available.
 * @private
 */
WebView.prototype.maybeSetupPermissionEvent_ = function() {};

/**
 * Implemented when experimental permission is available.
 * @private
 */
WebView.prototype.maybeSetupExecuteScript_ = function() {};

exports.WebView = WebView;
