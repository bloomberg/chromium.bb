// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var IdGenerator = requireNative('id_generator');
var guestViewInternalNatives = requireNative('guest_view_internal');

function AppViewInternal(appviewNode) {
  privates(appviewNode).internal = this;
  this.appviewNode = appviewNode;
  this.elementAttached = false;
  this.pendingGuestCreation = false;

  this.browserPluginNode = this.createBrowserPluginNode();
  var shadowRoot = this.appviewNode.createShadowRoot();
  shadowRoot.appendChild(this.browserPluginNode);
  this.viewInstanceId = IdGenerator.GetNextId();
}

AppViewInternal.prototype.getErrorNode = function() {
  if (!this.errorNode) {
    this.errorNode = document.createElement('div');
    this.errorNode.innerText = 'Unable to connect to app.';
    this.errorNode.style.position = 'absolute';
    this.errorNode.style.left = '0px';
    this.errorNode.style.top = '0px';
    this.errorNode.style.width = '100%';
    this.errorNode.style.height = '100%';
    this.appviewNode.shadowRoot.appendChild(this.errorNode);
  }
  return this.errorNode;
};

AppViewInternal.prototype.createBrowserPluginNode = function() {
  // We create BrowserPlugin as a custom element in order to observe changes
  // to attributes synchronously.
  var browserPluginNode = new AppViewInternal.BrowserPlugin();
  privates(browserPluginNode).internal = this;
  return browserPluginNode;
};

AppViewInternal.prototype.connect = function(app, data, callback) {
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
        this.browserPluginNode.style.visibility = 'hidden';
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

AppViewInternal.prototype.attachWindow = function(guestInstanceId) {
  this.guestInstanceId = guestInstanceId;
  if (!this.internalInstanceId) {
    return;
  }
  var params = {
    'instanceId': this.viewInstanceId
  };
  this.browserPluginNode.style.visibility = 'visible';
  return guestViewInternalNatives.AttachGuest(
      this.internalInstanceId,
      guestInstanceId,
      params);
};

AppViewInternal.prototype.handleBrowserPluginAttributeMutation =
    function(name, oldValue, newValue) {
  if (name == 'internalinstanceid' && !oldValue && !!newValue) {
    this.browserPluginNode.removeAttribute('internalinstanceid');
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

AppViewInternal.prototype.reset = function() {
  if (this.guestInstanceId) {
    GuestViewInternal.destroyGuest(this.guestInstanceId);
    this.guestInstanceId = undefined;
  }
};

function registerBrowserPluginElement() {
  var proto = Object.create(HTMLObjectElement.prototype);

  proto.createdCallback = function() {
    this.setAttribute('type', 'application/browser-plugin');
    this.style.width = '100%';
    this.style.height = '100%';
  };

  proto.attachedCallback = function() {
    // Load the plugin immediately.
    var unused = this.nonExistentAttribute;
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.handleBrowserPluginAttributeMutation(name, oldValue, newValue);
  };

  AppViewInternal.BrowserPlugin =
      DocumentNatives.RegisterElement('appplugin', {extends: 'object',
                                                    prototype: proto});

  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
}

function registerAppViewElement() {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    new AppViewInternal(this);
  };

  proto.attachedCallback = function() {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.elementAttached = true;
  };

  proto.detachedCallback = function() {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.elementAttached = false;
    internal.reset();
  };

  proto.connect = function() {
    var internal = privates(this).internal;
    $Function.apply(internal.connect, internal, arguments);
  }

  window.AppView =
      DocumentNatives.RegisterElement('appview', {prototype: proto});

  // Delete the callbacks so developers cannot call them and produce unexpected
  // behavior.
  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
}

var useCapture = true;
window.addEventListener('readystatechange', function listener(event) {
  if (document.readyState == 'loading')
    return;

  registerBrowserPluginElement();
  registerAppViewElement();
  window.removeEventListener(event.type, listener, useCapture);
}, useCapture);
