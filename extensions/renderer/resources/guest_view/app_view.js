// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');
var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var IdGenerator = requireNative('id_generator');

function AppViewImpl(appviewElement) {
  GuestViewContainer.call(this, appviewElement)

  this.guest = new GuestView('appview');

  var shadowRoot = this.element.createShadowRoot();
  shadowRoot.appendChild(this.browserPluginElement);
  this.viewInstanceId = IdGenerator.GetNextId();
}

AppViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

AppViewImpl.VIEW_TYPE = 'AppView';

// Add extra functionality to |this.element|.
AppViewImpl.setupElement = function(proto) {
  var apiMethods = [
    'connect'
  ];

  // Forward proto.foo* method calls to AppViewImpl.foo*.
  GuestViewContainer.forwardApiMethods(proto, apiMethods);
}

AppViewImpl.prototype.onElementDetached = function() {
  this.guest.destroy();
};

AppViewImpl.prototype.getErrorNode = function() {
  if (!this.errorNode) {
    this.errorNode = document.createElement('div');
    this.errorNode.innerText = 'Unable to connect to app.';
    this.errorNode.style.position = 'absolute';
    this.errorNode.style.left = '0px';
    this.errorNode.style.top = '0px';
    this.errorNode.style.width = '100%';
    this.errorNode.style.height = '100%';
    this.element.shadowRoot.appendChild(this.errorNode);
  }
  return this.errorNode;
};

AppViewImpl.prototype.connect = function(app, data, callback) {
  if (!this.elementAttached) {
    if (callback) {
      callback(false);
    }
    return;
  }
  var createParams = {
    'appId': app,
    'data': data || {}
  };

  this.guest.create(createParams, function() {
    if (!this.guest.getId()) {
      this.browserPluginElement.style.visibility = 'hidden';
      var errorMsg = 'Unable to connect to app "' + app + '".';
      window.console.warn(errorMsg);
      this.getErrorNode().innerText = errorMsg;
      if (callback) {
        callback(false);
      }
      return;
    }
    this.attachWindow();
    if (callback) {
      callback(true);
    }
  }.bind(this));
};

AppViewImpl.prototype.buildAttachParams = function() {
  var params = {
    'instanceId': this.viewInstanceId
  };
  return params;
};

AppViewImpl.prototype.attachWindow = function() {
  if (!this.internalInstanceId) {
    return true;
  }

  this.browserPluginElement.style.visibility = 'visible';
  this.guest.attach(this.internalInstanceId, this.buildAttachParams());
  return true;
};

AppViewImpl.prototype.handleBrowserPluginAttributeMutation =
    function(name, oldValue, newValue) {
  if (name == 'internalinstanceid' && !oldValue && !!newValue) {
    this.browserPluginElement.removeAttribute('internalinstanceid');
    this.internalInstanceId = parseInt(newValue);

    if (!this.guest.getId()) {
      return;
    }
    this.guest.attach(this.internalInstanceId, this.buildAttachParams());
  }
};

GuestViewContainer.registerElement(AppViewImpl);
