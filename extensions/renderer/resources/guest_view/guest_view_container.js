// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the shared functionality for different guestview
// containers, such as web_view, app_view, etc.

var DocumentNatives = requireNative('document_natives');
var GuestView = require('guestView').GuestView;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');

function GuestViewContainer(element, viewType) {
  privates(element).internal = this;
  this.element = element;
  this.elementAttached = false;
  this.viewInstanceId = IdGenerator.GetNextId();
  this.viewType = viewType;

  this.setupGuestProperty();
  this.guest = new GuestView(viewType);

  privates(this).browserPluginElement = this.createBrowserPluginElement();
  this.setupFocusPropagation();

  var shadowRoot = this.element.createShadowRoot();
  shadowRoot.appendChild(privates(this).browserPluginElement);
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
GuestViewContainer.registerElement =
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

// Create the 'guest' property to track new GuestViews and always listen for
// their resizes.
GuestViewContainer.prototype.setupGuestProperty = function() {
  Object.defineProperty(this, 'guest', {
    get: function() {
      return privates(this).guest;
    }.bind(this),
    set: function(value) {
      privates(this).guest = value;
      if (!value) {
        return;
      }
      privates(this).guest.onresize = function(e) {
        // Dispatch the 'contentresize' event.
        var contentResizeEvent = new Event('contentresize', { bubbles: true });
        contentResizeEvent.oldWidth = e.oldWidth;
        contentResizeEvent.oldHeight = e.oldHeight;
        contentResizeEvent.newWidth = e.newWidth;
        contentResizeEvent.newHeight = e.newHeight;
        this.dispatchEvent(contentResizeEvent);
      }.bind(this);
    }.bind(this),
    enumerable: true
  });
};

GuestViewContainer.prototype.createBrowserPluginElement = function() {
  // We create BrowserPlugin as a custom element in order to observe changes
  // to attributes synchronously.
  var browserPluginElement =
      new GuestViewContainer[this.viewType + 'BrowserPlugin']();
  privates(browserPluginElement).internal = this;
  return browserPluginElement;
};

GuestViewContainer.prototype.setupFocusPropagation = function() {
  if (!this.element.hasAttribute('tabIndex')) {
    // GuestViewContainer needs a tabIndex in order to be focusable.
    // TODO(fsamuel): It would be nice to avoid exposing a tabIndex attribute
    // to allow GuestViewContainer to be focusable.
    // See http://crbug.com/231664.
    this.element.setAttribute('tabIndex', -1);
  }
  this.element.addEventListener('focus', function(e) {
    // Focus the BrowserPlugin when the GuestViewContainer takes focus.
    privates(this).browserPluginElement.focus();
  }.bind(this));
  this.element.addEventListener('blur', function(e) {
    // Blur the BrowserPlugin when the GuestViewContainer loses focus.
    privates(this).browserPluginElement.blur();
  }.bind(this));
};

GuestViewContainer.prototype.attachWindow = function() {
  if (!this.internalInstanceId) {
    return true;
  }

  this.guest.attach(this.internalInstanceId,
                    this.viewInstanceId,
                    this.buildParams());
  return true;
};

GuestViewContainer.prototype.handleBrowserPluginAttributeMutation =
    function(name, oldValue, newValue) {
  if (name == 'internalinstanceid' && !oldValue && !!newValue) {
    privates(this).browserPluginElement.removeAttribute('internalinstanceid');
    this.internalInstanceId = parseInt(newValue);

    // Track when the element resizes using the element resize callback.
    GuestViewInternalNatives.RegisterElementResizeCallback(
        this.internalInstanceId, this.onElementResize.bind(this));

    if (!this.guest.getId()) {
      return;
    }
    this.guest.attach(this.internalInstanceId,
                      this.viewInstanceId,
                      this.buildParams());
  }
};

GuestViewContainer.prototype.onElementResize = function(oldWidth, oldHeight,
                                                        newWidth, newHeight) {
  // Dispatch the 'resize' event.
  var resizeEvent = new Event('resize', { bubbles: true });
  resizeEvent.oldWidth = oldWidth;
  resizeEvent.oldHeight = oldHeight;
  resizeEvent.newWidth = newWidth;
  resizeEvent.newHeight = newHeight;
  this.dispatchEvent(resizeEvent);

  if (!this.guest.getId())
    return;
  this.guest.setSize({normal: {width: newWidth, height: newHeight}});
};

GuestViewContainer.prototype.buildParams = function() {
  var params = this.buildContainerParams();
  params['instanceId'] = this.viewInstanceId;
  var elementRect = this.element.getBoundingClientRect();
  params['elementWidth'] = parseInt(elementRect.width);
  params['elementHeight'] = parseInt(elementRect.height);
  return params;
};

GuestViewContainer.prototype.dispatchEvent = function(event) {
  return this.element.dispatchEvent(event);
}

// Implemented by the specific view type, if needed.
GuestViewContainer.prototype.buildContainerParams = function() { return {}; };
GuestViewContainer.prototype.handleAttributeMutation = function() {};
GuestViewContainer.prototype.onElementAttached = function() {};
GuestViewContainer.prototype.onElementDetached = function() {
  this.guest.destroy();
};

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

  GuestViewContainer[viewType + 'BrowserPlugin'] =
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
    internal.internalInstanceId = 0;
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
