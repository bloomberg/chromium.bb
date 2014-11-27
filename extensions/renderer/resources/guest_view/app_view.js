// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var IdGenerator = requireNative('id_generator');
var guestViewInternalNatives = requireNative('guest_view_internal');

function AppViewImpl(appviewElement) {
  GuestViewContainer.call(this, appviewElement)

  this.pendingGuestCreation = false;

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
  if (this.guestInstanceId) {
    GuestViewInternal.destroyGuest(this.guestInstanceId);
    this.guestInstanceId = undefined;
  }
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
  if (!this.elementAttached || this.pendingGuestCreation) {
    if (callback) {
      callback(false);
    }
    return;
  }
  var createParams = {
    'appId': app,
    'data': data || {}
  };
  GuestViewInternal.createGuest(
    'appview',
    createParams,
    function(guestInstanceId) {
      this.pendingGuestCreation = false;
      if (guestInstanceId && !this.elementAttached) {
        GuestViewInternal.destroyGuest(guestInstanceId);
        guestInstanceId = 0;
      }
      if (!guestInstanceId) {
        this.browserPluginElement.style.visibility = 'hidden';
        var errorMsg = 'Unable to connect to app "' + app + '".';
        window.console.warn(errorMsg);
        this.getErrorNode().innerText = errorMsg;
        if (callback) {
          callback(false);
        }
        return;
      }
      this.attachWindow(guestInstanceId);
      if (callback) {
        callback(true);
      }
    }.bind(this)
  );
  this.pendingGuestCreation = true;
};

AppViewImpl.prototype.attachWindow = function(guestInstanceId) {
  this.guestInstanceId = guestInstanceId;
  if (!this.internalInstanceId) {
    return;
  }
  var params = {
    'instanceId': this.viewInstanceId
  };
  this.browserPluginElement.style.visibility = 'visible';
  return guestViewInternalNatives.AttachGuest(
      this.internalInstanceId,
      guestInstanceId,
      params);
};

AppViewImpl.prototype.handleBrowserPluginAttributeMutation =
    function(name, oldValue, newValue) {
  if (name == 'internalinstanceid' && !oldValue && !!newValue) {
    this.browserPluginElement.removeAttribute('internalinstanceid');
    this.internalInstanceId = parseInt(newValue);

    if (!!this.guestInstanceId && this.guestInstanceId != 0) {
      var params = {
        'instanceId': this.viewInstanceId
      };
      guestViewInternalNatives.AttachGuest(
          this.internalInstanceId,
          this.guestInstanceId,
          params);
    }
    return;
  }
};

GuestViewContainer.listenForReadyStateChange(AppViewImpl);
