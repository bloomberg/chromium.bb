// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');
var ExtensionOptionsEvents =
    require('extensionOptionsEvents').ExtensionOptionsEvents;
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var IdGenerator = requireNative('id_generator');
var utils = require('utils');
var guestViewInternalNatives = requireNative('guest_view_internal');

// Mapping of the autosize attribute names to default values
var AUTO_SIZE_ATTRIBUTES = {
  'autosize': 'on',
  'maxheight': window.innerHeight,
  'maxwidth': window.innerWidth,
  'minheight': 32,
  'minwidth': 32
};

function ExtensionOptionsInternal(extensionoptionsNode) {
  privates(extensionoptionsNode).internal = this;
  this.extensionoptionsNode = extensionoptionsNode;
  this.viewInstanceId = IdGenerator.GetNextId();

  // on* Event handlers.
  this.eventHandlers = {};

  // setupEventProperty is normally called in extension_options_events.js to
  // register events, but the createfailed event is registered here because
  // the event is fired from here instead of through
  // extension_options_events.js.
  this.setupEventProperty('createfailed');

  new ExtensionOptionsEvents(this, this.viewInstanceId);

  this.setupNodeProperties();

  if (this.parseExtensionAttribute())
    this.init();
};

ExtensionOptionsInternal.prototype.attachWindow = function(guestInstanceId) {
  this.guestInstanceId = guestInstanceId;
  var params = {
    'autosize': this.autosize,
    'instanceId': this.viewInstanceId,
    'maxheight': parseInt(this.maxheight || 0),
    'maxwidth': parseInt(this.maxwidth || 0),
    'minheight': parseInt(this.minheight || 0),
    'minwidth': parseInt(this.minwidth || 0)
  };
  return guestViewInternalNatives.AttachGuest(
      parseInt(this.browserPluginNode.getAttribute('internalinstanceid')),
      guestInstanceId,
      params);
};

ExtensionOptionsInternal.prototype.createBrowserPluginNode = function() {
  var browserPluginNode = new ExtensionOptionsInternal.BrowserPlugin();
  privates(browserPluginNode).internal = this;
  return browserPluginNode;
};

ExtensionOptionsInternal.prototype.createGuest = function() {
  var params = {
    'extensionId': this.extensionId,
  };
  GuestViewInternal.createGuest(
      'extensionoptions',
      params,
      function(guestInstanceId) {
        if (guestInstanceId == 0) {
          // Fire a createfailed event here rather than in ExtensionOptionsGuest
          // because the guest will not be created, and cannot fire an event.
          this.initCalled = false;
          var createFailedEvent = new Event('createfailed', { bubbles: true });
          this.dispatchEvent(createFailedEvent);
        } else {
          this.attachWindow(guestInstanceId);
        }
      }.bind(this));
};

ExtensionOptionsInternal.prototype.dispatchEvent =
    function(extensionOptionsEvent) {
  return this.extensionoptionsNode.dispatchEvent(extensionOptionsEvent);
};

ExtensionOptionsInternal.prototype.handleExtensionOptionsAttributeMutation =
    function(name, oldValue, newValue) {
  // We treat null attribute (attribute removed) and the empty string as
  // one case.
  oldValue = oldValue || '';
  newValue = newValue || '';

  if (oldValue === newValue)
    return;

  if (name == 'extension') {
    this.extensionId = newValue;
    // Create new guest view if one hasn't been created for this element.
    if (!this.guestInstanceId && this.parseExtensionAttribute())
      this.init();
    // TODO(ericzeng): Implement navigation to another guest view if we want
    // that functionality.
  } else if (AUTO_SIZE_ATTRIBUTES.hasOwnProperty(name) > -1) {
    this[name] = newValue;
    this.resetSizeConstraintsIfInvalid();

    if (!this.guestInstanceId)
      return;

    GuestViewInternal.setAutoSize(this.guestInstanceId, {
      'enableAutoSize': this.extensionoptionsNode.hasAttribute('autosize'),
      'min': {
        'width': parseInt(this.minwidth || 0),
        'height': parseInt(this.minheight || 0)
      },
      'max': {
        'width': parseInt(this.maxwidth || 0),
        'height': parseInt(this.maxheight || 0)
      }
    });
  }
};

ExtensionOptionsInternal.prototype.init = function() {
  if (this.initCalled)
    return;

  this.initCalled = true;
  this.browserPluginNode = this.createBrowserPluginNode();
  var shadowRoot = this.extensionoptionsNode.createShadowRoot();
  shadowRoot.appendChild(this.browserPluginNode);
  this.createGuest();
};

ExtensionOptionsInternal.prototype.onSizeChanged = function(width, height) {
  this.browserPluginNode.style.width = width + 'px';
  this.browserPluginNode.style.height = height + 'px';
};

ExtensionOptionsInternal.prototype.parseExtensionAttribute = function() {
  if (this.extensionoptionsNode.hasAttribute('extension')) {
    this.extensionId = this.extensionoptionsNode.getAttribute('extension');
    return true;
  }
  return false;
};

// Adds an 'on<event>' property on the view, which can be used to set/unset
// an event handler.
ExtensionOptionsInternal.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  var extensionoptionsNode = this.extensionoptionsNode;
  Object.defineProperty(extensionoptionsNode, propertyName, {
    get: function() {
      return this.eventHandlers[propertyName];
    }.bind(this),
    set: function(value) {
      if (this.eventHandlers[propertyName])
        extensionoptionsNode.removeEventListener(
            eventName, this.eventHandlers[propertyName]);
      this.eventHandlers[propertyName] = value;
      if (value)
        extensionoptionsNode.addEventListener(eventName, value);
    }.bind(this),
    enumerable: true
  });
};

ExtensionOptionsInternal.prototype.setupNodeProperties = function() {
  utils.forEach(AUTO_SIZE_ATTRIBUTES, function(attributeName) {
    // Get the size constraints from the <extensionoptions> tag, or use the
    // defaults if not specified
    if (this.extensionoptionsNode.hasAttribute(attributeName)) {
      this[attributeName] =
          this.extensionoptionsNode.getAttribute(attributeName);
    } else {
      this[attributeName] = AUTO_SIZE_ATTRIBUTES[attributeName];
    }

    Object.defineProperty(this.extensionoptionsNode, attributeName, {
      get: function() {
        return this[attributeName];
      }.bind(this),
      set: function(value) {
        this.extensionoptionsNode.setAttribute(attributeName, value);
      }.bind(this),
      enumerable: true
    });
  }, this);

  this.resetSizeConstraintsIfInvalid();

  Object.defineProperty(this.extensionoptionsNode, 'extension', {
    get: function() {
      return this.extensionId;
    }.bind(this),
    set: function(value) {
      this.extensionoptionsNode.setAttribute('extension', value);
    }.bind(this),
    enumerable: true
  });
};

ExtensionOptionsInternal.prototype.resetSizeConstraintsIfInvalid = function () {
  if (this.minheight > this.maxheight || this.minheight < 0) {
    this.minheight = AUTO_SIZE_ATTRIBUTES.minheight;
    this.maxheight = AUTO_SIZE_ATTRIBUTES.maxheight;
  }
  if (this.minwidth > this.maxwidth || this.minwidth < 0) {
    this.minwidth = AUTO_SIZE_ATTRIBUTES.minwidth;
    this.maxwidth = AUTO_SIZE_ATTRIBUTES.maxwidth;
  }
}

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

  ExtensionOptionsInternal.BrowserPlugin =
      DocumentNatives.RegisterElement('extensionoptionsplugin',
                                       {extends: 'object', prototype: proto});
  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
}

function registerExtensionOptionsElement() {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    new ExtensionOptionsInternal(this);
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal)
      return;
    internal.handleExtensionOptionsAttributeMutation(name, oldValue, newValue);
  };

  window.ExtensionOptions =
      DocumentNatives.RegisterElement('extensionoptions', {prototype: proto});

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
  registerExtensionOptionsElement();
  window.removeEventListener(event.type, listener, useCapture);
}, useCapture);
