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

// Mapping of the autosize attribute names to default values
var AUTO_SIZE_ATTRIBUTES = {
  'autosize': 'on',
  'maxheight': 600,
  'maxwidth': 800,
  'minheight': 32,
  'minwidth': 80
};

function ExtensionOptionsInternal(extensionoptionsNode) {
  privates(extensionoptionsNode).internal = this;
  this.extensionoptionsNode = extensionoptionsNode;
  this.viewInstanceId = IdGenerator.GetNextId();

  // on* Event handlers.
  this.eventHandlers = {};
  new ExtensionOptionsEvents(this, this.viewInstanceId);

  this.setupNodeProperties();

  if (this.parseExtensionAttribute())
    this.init();
};

ExtensionOptionsInternal.prototype.attachWindow = function(instanceId) {
  this.instanceId = instanceId;
  var params = {
    'autosize': this.autosize,
    'instanceId': this.viewInstanceId,
    'maxheight': parseInt(this.maxheight || 0),
    'maxwidth': parseInt(this.maxwidth || 0),
    'minheight': parseInt(this.minheight || 0),
    'minwidth': parseInt(this.minwidth || 0)
  }
  return this.browserPluginNode['-internal-attach'](instanceId, params);
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
      function(instanceId) {
        if (instanceId == 0) {
          this.initCalled = false;
        } else {
          this.attachWindow(instanceId);
          GuestViewInternal.setAutoSize(this.instanceId, {
            'enableAutoSize':
                this.extensionoptionsNode.hasAttribute('autosize'),
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
    if (!this.instanceId && this.parseExtensionAttribute())
      this.init();
    // TODO(ericzeng): Implement navigation to another guest view if we want
    // that functionality.
  } else if (AUTO_SIZE_ATTRIBUTES.hasOwnProperty(name) > -1) {
    this[name] = newValue;
    this.resetSizeConstraintsIfInvalid();

    if (!this.instanceId)
      return;

    GuestViewInternal.setAutoSize(this.instanceId, {
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
    var extensionId = this.extensionoptionsNode.getAttribute('extension');
    // Only allow extensions to embed their own options page (if it has one).
    if (chrome.runtime.id == extensionId &&
        chrome.runtime.getManifest().hasOwnProperty('options_page')) {
      this.extensionId  = extensionId;
      return true;
    }
  }
  return false;
};

// Adds an 'on<event>' property on the view, which can be used to set/unset
// an event handler.
ExtensionOptionsInternal.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  var self = this;
  var extensionoptionsNode = this.extensionoptionsNode;
  Object.defineProperty(extensionoptionsNode, propertyName, {
    get: function() {
      return self.eventHandlers[propertyName];
    },
    set: function(value) {
      if (self.eventHandlers[propertyName])
        extensionoptionsNode.removeEventListener(
            eventName, self.eventHandlers[propertyName]);
      self.eventHandlers[propertyName] = value;
      if (value)
        extensionoptionsNode.addEventListener(eventName, value);
    },
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
