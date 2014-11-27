// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the shared functionality for different guestview
// containers, such as web_view, app_view, etc.

var DocumentNatives = requireNative('document_natives');
var IdGenerator = requireNative('id_generator');

function GuestViewContainer(element) {
  privates(element).internal = this;
  this.element = element;
  this.elementAttached = false;

  this.browserPluginElement = this.createBrowserPluginElement();
}

// Forward public API methods from |proto| to their internal implementations.
GuestViewContainer.forwardApiMethods = function(proto, apiMethods) {
  var createProtoHandler = function(m) {
    return function(var_args) {
      var internal = privates(this).internal;
      return $Function.apply(internal[m], internal, arguments);
    };
  };
  for (var i = 0; apiMethods[i]; ++i) {
    proto[apiMethods[i]] = createProtoHandler(apiMethods[i]);
  }
};

// Registers the browserplugin and guestview as custom elements once the
// document has loaded.
GuestViewContainer.listenForReadyStateChange =
    function(guestViewContainerType) {
  var useCapture = true;
  window.addEventListener('readystatechange', function listener(event) {
    if (document.readyState == 'loading') {
      return;
    }

    registerBrowserPluginElement(
        guestViewContainerType.VIEW_TYPE.toLowerCase());
    registerGuestViewElement(guestViewContainerType);
    window.removeEventListener(event.type, listener, useCapture);
  }, useCapture);
};

GuestViewContainer.prototype.createBrowserPluginElement = function() {
  // We create BrowserPlugin as a custom element in order to observe changes
  // to attributes synchronously.
  var browserPluginElement = new GuestViewContainer.BrowserPlugin();
  privates(browserPluginElement).internal = this;
  return browserPluginElement;
};

// Implemented by the specific view type.
GuestViewContainer.prototype.handleAttributeMutation = function() {};
GuestViewContainer.prototype.onElementAttached = function() {};
GuestViewContainer.prototype.onElementDetached = function() {};

// Registers the browser plugin <object> custom element. |viewType| is the
// name of the specific guestview container (e.g. 'webview').
function registerBrowserPluginElement(viewType) {
  var proto = Object.create(HTMLObjectElement.prototype);

  proto.createdCallback = function() {
    this.setAttribute('type', 'application/browser-plugin');
    this.setAttribute('id', 'browser-plugin-' + IdGenerator.GetNextId());
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

  GuestViewContainer.BrowserPlugin =
      DocumentNatives.RegisterElement(viewType + 'browserplugin',
                                      {extends: 'object', prototype: proto});

  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
};

// Registers the guestview container as a custom element.
// |guestViewContainerType| is the type of guestview container
// (e.g.WebViewImpl).
function registerGuestViewElement(guestViewContainerType) {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    new guestViewContainerType(this);
  };

  proto.attachedCallback = function() {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.elementAttached = true;
    internal.onElementAttached();
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.handleAttributeMutation(name, oldValue, newValue);
  };

  proto.detachedCallback = function() {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.elementAttached = false;
    internal.onElementDetached();
  };

  // Let the specific view type add extra functionality to its custom element
  // through |proto|.
  if (guestViewContainerType.setupElement) {
    guestViewContainerType.setupElement(proto);
  }

  window[guestViewContainerType.VIEW_TYPE] =
      DocumentNatives.RegisterElement(
          guestViewContainerType.VIEW_TYPE.toLowerCase(),
          {prototype: proto});

  // Delete the callbacks so developers cannot call them and produce unexpected
  // behavior.
  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
}

// Exports.
exports.GuestViewContainer = GuestViewContainer;
