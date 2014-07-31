// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');
var ExtensionOptionsEvents =
    require('extensionOptionsEvents').ExtensionOptionsEvents;
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var IdGenerator = requireNative('id_generator');

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
    'instanceId': this.viewInstanceId,
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
  var self = this;
  GuestViewInternal.createGuest(
      'extensionoptions',
      params,
      function(instanceId) {
        if (instanceId == 0) {
          self.initCalled = false;
        } else {
          self.attachWindow(instanceId);
        }
      });
};

ExtensionOptionsInternal.prototype.dispatchEvent =
    function(extensionOptionsEvent) {
  return this.extensionoptionsNode.dispatchEvent(extensionOptionsEvent);
};

ExtensionOptionsInternal.prototype.handleExtensionOptionsAttributeMutation =
    function(name, oldValue, newValue) {
  if (name != 'extension')
    return;
  // We treat null attribute (attribute removed) and the empty string as
  // one case.
  oldValue = oldValue || '';
  newValue = newValue || '';

  if (oldValue === newValue)
    return;
  this.extensionId = newValue;

  // Create new guest view if one hasn't been created for this element.
  if (!this.instanceId && this.parseExtensionAttribute())
    this.init();
  // TODO(ericzeng): Implement navigation to another guest view if we want
  // that functionality.
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
  var self = this;
  this.extensionId = this.extensionoptionsNode.getAttribute('extension');
  Object.defineProperty(this.extensionoptionsNode, 'extension', {
    get: function() {
      return self.extensionId;
    },
    set: function(value) {
      self.extensionoptionsNode.setAttribute('extension', value);
    },
    enumerable: true
  });
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
