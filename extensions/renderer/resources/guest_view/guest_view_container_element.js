// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common custom element registration code for the various guest view
// containers.

var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var DocumentNatives = requireNative('document_natives');
var IdGenerator = requireNative('id_generator');

// Registers the browserplugin and guestview as custom elements once the
// document has loaded.
// |containerElementType| is a GuestViewContainerElement (e.g. WebViewElement)
// |containerType| is a GuestViewContainer (e.g. WebViewImpl)
function registerElement(elementName, containerElementType, containerType) {
  var useCapture = true;
  window.addEventListener('readystatechange', function listener(event) {
    if (document.readyState == 'loading')
      return;

    registerInternalElement($String.toLowerCase(elementName));
    registerGuestViewElement(elementName, containerElementType, containerType);
    window.removeEventListener(event.type, listener, useCapture);
  }, useCapture);
}

// Registers the browser plugin <object> custom element. |viewType| is the
// name of the specific guestview container (e.g. 'webview').
function registerInternalElement(viewType) {
  var InternalElement = class extends HTMLObjectElement {}

  InternalElement.prototype.createdCallback = function() {
    this.setAttribute('type', 'application/browser-plugin');
    this.setAttribute('id', 'browser-plugin-' + IdGenerator.GetNextId());
    this.style.width = '100%';
    this.style.height = '100%';
  };

  InternalElement.prototype.attachedCallback = function() {
    // Load the plugin immediately.
    var unused = this.nonExistentAttribute;
  };

  InternalElement.prototype.attributeChangedCallback = function(
      name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.handleInternalElementAttributeMutation(name, oldValue, newValue);
  };

  GuestViewContainer[viewType + 'BrowserPlugin'] =
      DocumentNatives.RegisterElement(
          viewType + 'browserplugin',
          {extends: 'object', prototype: InternalElement.prototype});

  delete InternalElement.prototype.createdCallback;
  delete InternalElement.prototype.attachedCallback;
  delete InternalElement.prototype.attributeChangedCallback;
}

// Conceptually, these are methods on GuestViewContainerElement.prototype.
// However, since that is exposed to users, we only set these callbacks on
// the prototype temporarily during the custom element registration.
var customElementCallbacks = {
  makeCreatedCallback: function(containerType) {
    return function() {
      privates(this).internal = new containerType(this);
    };
  },

  attachedCallback: function() {
    var internal = privates(this).internal;
    if (!internal)
      return;

    internal.elementAttached = true;
    internal.willAttachElement();
    internal.onElementAttached();
  },

  attributeChangedCallback: function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal || !internal.attributes[name])
      return;

    // Let the changed attribute handle its own mutation.
    internal.attributes[name].maybeHandleMutation(oldValue, newValue);
  },

  detachedCallback: function() {
    var internal = privates(this).internal;
    if (!internal)
      return;

    internal.elementAttached = false;
    internal.internalInstanceId = 0;
    internal.guest.destroy();
    internal.onElementDetached();
  }
};

// Registers a GuestViewContainerElement as a custom element.
function registerGuestViewElement(
    elementName, containerElementType, containerType) {
  // We set the lifecycle callbacks so that they're available during
  // registration. Once that's done, we'll delete them so developers cannot
  // call them and produce unexpected behaviour.
  GuestViewContainerElement.prototype.createdCallback =
      customElementCallbacks.makeCreatedCallback(containerType);
  GuestViewContainerElement.prototype.attachedCallback =
      customElementCallbacks.attachedCallback;
  GuestViewContainerElement.prototype.detachedCallback =
      customElementCallbacks.detachedCallback;
  GuestViewContainerElement.prototype.attributeChangedCallback =
      customElementCallbacks.attributeChangedCallback;

  window[elementName] = DocumentNatives.RegisterElement(
      $String.toLowerCase(elementName),
      {prototype: containerElementType.prototype});

  delete GuestViewContainerElement.prototype.createdCallback;
  delete GuestViewContainerElement.prototype.attachedCallback;
  delete GuestViewContainerElement.prototype.detachedCallback;
  delete GuestViewContainerElement.prototype.attributeChangedCallback;
}

// Forward public API methods from |containerElementType|'s prototype to their
// internal implementations.
function forwardApiMethods(containerElementType, methodNames) {
  var createProtoHandler = function(m) {
    return function(var_args) {
      var internal = privates(this).internal;
      return $Function.apply(internal[m], internal, arguments);
    };
  };
  for (var m of methodNames) {
    containerElementType.prototype[m] = createProtoHandler(m);
  }
}

class GuestViewContainerElement extends HTMLElement {}

// Override |focus| to let |internal| handle it.
GuestViewContainerElement.prototype.focus = function() {
  var internal = privates(this).internal;
  if (!internal)
    return;

  internal.focus();
};

// Exports.
exports.$set('GuestViewContainerElement', GuestViewContainerElement);
exports.$set('registerElement', registerElement);
exports.$set('forwardApiMethods', forwardApiMethods);
