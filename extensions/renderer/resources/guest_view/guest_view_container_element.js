// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common custom element registration code for the various guest view
// containers.

var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var logging = requireNative('logging');

// Registers the browserplugin and guestview as custom elements once the
// document has loaded.
// |containerElementType| is a GuestViewContainerElement (e.g. WebViewElement)
function registerElement(elementName, containerElementType) {
  var useCapture = true;
  window.addEventListener('readystatechange', function listener(event) {
    if (document.readyState == 'loading')
      return;

    registerInternalElement($String.toLowerCase(elementName));
    registerGuestViewElement(elementName, containerElementType);
    window.removeEventListener(event.type, listener, useCapture);
  }, useCapture);
}

// Registers the browser plugin <object> custom element. |viewType| is the
// name of the specific guestview container (e.g. 'webview').
function registerInternalElement(viewType) {
  GuestViewInternalNatives.AllowGuestViewElementDefinition(() => {
    var InternalElement = class extends HTMLObjectElement {
      static get observedAttributes() {
        return ['internalinstanceid'];
      }

      constructor() {
        super();
        this.setAttribute('type', 'application/browser-plugin');
        this.setAttribute('id', 'browser-plugin-' + IdGenerator.GetNextId());
        this.style.width = '100%';
        this.style.height = '100%';
      }
    }

    InternalElement.prototype.connectedCallback = function() {
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

    window.customElements.define(
        viewType + 'browserplugin', InternalElement, {extends: 'object'});
    GuestViewContainer[viewType + 'BrowserPlugin'] = InternalElement;

    delete InternalElement.prototype.connectedCallback;
    delete InternalElement.prototype.attributeChangedCallback;

    delete InternalElement.observedAttributes;
  });
}

// Conceptually, these are methods on GuestViewContainerElement.prototype.
// However, since that is exposed to users, we only set these callbacks on
// the prototype temporarily during the custom element registration.
var customElementCallbacks = {
  connectedCallback: function() {
    var internal = privates(this).internal;
    if (!internal)
      return;

    internal.elementAttached = true;
    internal.willAttachElement();
    internal.onElementAttached();
  },

  attributeChangedCallback: function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal)
      return;

    // Let the changed attribute handle its own mutation.
    internal.attributes[name].maybeHandleMutation(oldValue, newValue);
  },

  disconnectedCallback: function() {
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
function registerGuestViewElement(elementName, containerElementType) {
  GuestViewInternalNatives.AllowGuestViewElementDefinition(() => {
    // We set the lifecycle callbacks so that they're available during
    // registration. Once that's done, we'll delete them so developers cannot
    // call them and produce unexpected behaviour.
    GuestViewContainerElement.prototype.connectedCallback =
        customElementCallbacks.connectedCallback;
    GuestViewContainerElement.prototype.disconnectedCallback =
        customElementCallbacks.disconnectedCallback;
    GuestViewContainerElement.prototype.attributeChangedCallback =
        customElementCallbacks.attributeChangedCallback;

    window.customElements.define(
        $String.toLowerCase(elementName), containerElementType);
    window[elementName] = containerElementType;

    delete GuestViewContainerElement.prototype.connectedCallback;
    delete GuestViewContainerElement.prototype.disconnectedCallback;
    delete GuestViewContainerElement.prototype.attributeChangedCallback;

    // Now that |observedAttributes| has been retrieved, we can hide it from
    // user code as well.
    delete containerElementType.observedAttributes;
  });
}

// Forward public API methods from |containerElementType|'s prototype to their
// internal implementations. If the method is defined on |containerType|, we
// forward to that. Otherwise, we forward to the method on |internalApi|.
function forwardApiMethods(
    containerElementType, containerType, internalApi, methodNames) {
  var createContainerImplHandler = function(m) {
    return function(var_args) {
      var internal = privates(this).internal;
      return $Function.apply(internal[m], internal, arguments);
    };
  };

  var createInternalApiHandler = function(m) {
    return function(var_args) {
      var internal = privates(this).internal;
      var instanceId = internal.guest.getId();
      if (!instanceId) {
        return false;
      }
      var args = $Array.concat([instanceId], $Array.slice(arguments));
      $Function.apply(internalApi[m], null, args);
      return true;
    };
  };

  for (var m of methodNames) {
    if (!containerElementType.prototype[m]) {
      if (containerType.prototype[m]) {
        containerElementType.prototype[m] = createContainerImplHandler(m);
      } else if (internalApi && internalApi[m]) {
        containerElementType.prototype[m] = createInternalApiHandler(m);
      } else {
        logging.DCHECK(false, m + ' has no implementation.');
      }
    }
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
