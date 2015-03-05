// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DocumentNatives = requireNative('document_natives');
var ExtensionOptionsEvents =
    require('extensionOptionsEvents').ExtensionOptionsEvents;
var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var IdGenerator = requireNative('id_generator');
var utils = require('utils');

// Mapping of the autosize attribute names to default values
var AUTO_SIZE_ATTRIBUTES = {
  'autosize': 'on',
  'maxheight': window.innerHeight,
  'maxwidth': window.innerWidth,
  'minheight': 32,
  'minwidth': 32
};

function ExtensionOptionsImpl(extensionoptionsElement) {
  GuestViewContainer.call(this, extensionoptionsElement, 'extensionoptions');
  this.autosizeDeferred = false;

  new ExtensionOptionsEvents(this);
  this.setupElementProperties();
};

ExtensionOptionsImpl.prototype.__proto__ = GuestViewContainer.prototype;

ExtensionOptionsImpl.VIEW_TYPE = 'ExtensionOptions';

// Add extra functionality to |this.element|.
ExtensionOptionsImpl.setupElement = function(proto) {
  var apiMethods = [
    'setDeferAutoSize',
    'resumeDeferredAutoSize'
  ];

  // Forward proto.foo* method calls to ExtensionOptionsImpl.foo*.
  GuestViewContainer.forwardApiMethods(proto, apiMethods);
}

ExtensionOptionsImpl.prototype.onElementAttached = function() {
  this.createGuest();
}

ExtensionOptionsImpl.prototype.buildContainerParams = function() {
  var params = {
    'autosize': this.element.hasAttribute('autosize'),
    'maxheight': parseInt(this.maxheight || 0),
    'maxwidth': parseInt(this.maxwidth || 0),
    'minheight': parseInt(this.minheight || 0),
    'minwidth': parseInt(this.minwidth || 0),
    'extensionId': this.element.getAttribute('extension')
  };
  return params;
};

ExtensionOptionsImpl.prototype.createGuest = function() {
  if (!this.elementAttached) {
    return;
  }

  // Destroy the old guest if one exists.
  this.guest.destroy();

  this.guest.create(this.buildParams(), function() {
    if (!this.guest.getId()) {
      // Fire a createfailed event here rather than in ExtensionOptionsGuest
      // because the guest will not be created, and cannot fire an event.
      var createFailedEvent = new Event('createfailed', { bubbles: true });
      this.dispatchEvent(createFailedEvent);
    } else {
      this.attachWindow();
    }
  }.bind(this));
};

ExtensionOptionsImpl.prototype.handleAttributeMutation =
    function(name, oldValue, newValue) {
  // We treat null attribute (attribute removed) and the empty string as
  // one case.
  oldValue = oldValue || '';
  newValue = newValue || '';

  if (oldValue === newValue)
    return;

  if (name == 'extension') {
    this.createGuest();
  } else if (AUTO_SIZE_ATTRIBUTES.hasOwnProperty(name) > -1) {
    this[name] = newValue;
    this.resetSizeConstraintsIfInvalid();

    if (!this.guest.getId())
      return;

    this.guest.setSize({
      'enableAutoSize': this.element.hasAttribute('autosize'),
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

ExtensionOptionsImpl.prototype.onSizeChanged =
    function(newWidth, newHeight, oldWidth, oldHeight) {
  if (this.autosizeDeferred) {
    this.deferredAutoSizeState = {
      newWidth: newWidth,
      newHeight: newHeight,
      oldWidth: oldWidth,
      oldHeight: oldHeight
    };
  } else {
    this.resize(newWidth, newHeight, oldWidth, oldHeight);
  }
};

ExtensionOptionsImpl.prototype.resize =
    function(newWidth, newHeight, oldWidth, oldHeight) {
  this.element.style.width = newWidth + 'px';
  this.element.style.height = newHeight + 'px';

  // Do not allow the options page's dimensions to shrink. This ensures that the
  // options page has a consistent UI. If the new size is larger than the
  // minimum, make that the new minimum size.
  if (newWidth > this.minwidth)
    this.minwidth = newWidth;
  if (newHeight > this.minheight)
    this.minheight = newHeight;

  this.guest.setSize({
    'enableAutoSize': this.element.hasAttribute('autosize'),
    'min': {
      'width': parseInt(this.minwidth || 0),
      'height': parseInt(this.minheight || 0)
    },
    'max': {
      'width': parseInt(this.maxwidth || 0),
      'height': parseInt(this.maxheight || 0)
    }
  });
};

ExtensionOptionsImpl.prototype.setupElementProperties = function() {
  utils.forEach(AUTO_SIZE_ATTRIBUTES, function(attributeName) {
    // Get the size constraints from the <extensionoptions> tag, or use the
    // defaults if not specified
    if (this.element.hasAttribute(attributeName)) {
      this[attributeName] =
          this.element.getAttribute(attributeName);
    } else {
      this[attributeName] = AUTO_SIZE_ATTRIBUTES[attributeName];
    }

    Object.defineProperty(this.element, attributeName, {
      get: function() {
        return this[attributeName];
      }.bind(this),
      set: function(value) {
        this.element.setAttribute(attributeName, value);
      }.bind(this),
      enumerable: true
    });
  }, this);

  this.resetSizeConstraintsIfInvalid();

  Object.defineProperty(this.element, 'extension', {
    get: function() {
      return this.element.getAttribute('extension');
    }.bind(this),
    set: function(value) {
      this.element.setAttribute('extension', value);
    }.bind(this),
    enumerable: true
  });
};

ExtensionOptionsImpl.prototype.resetSizeConstraintsIfInvalid = function () {
  if (this.minheight > this.maxheight || this.minheight < 0) {
    this.minheight = AUTO_SIZE_ATTRIBUTES.minheight;
    this.maxheight = AUTO_SIZE_ATTRIBUTES.maxheight;
  }
  if (this.minwidth > this.maxwidth || this.minwidth < 0) {
    this.minwidth = AUTO_SIZE_ATTRIBUTES.minwidth;
    this.maxwidth = AUTO_SIZE_ATTRIBUTES.maxwidth;
  }
};

/**
 * Toggles whether the element should automatically resize to its preferred
 * size. If set to true, when the element receives new autosize dimensions,
 * it passes them to the embedder in a sizechanged event, but does not resize
 * itself to those dimensions until the embedder calls resumeDeferredAutoSize.
 * This allows the embedder to defer the resizing until it is ready.
 * When set to false, the element resizes whenever it receives new autosize
 * dimensions.
 */
ExtensionOptionsImpl.prototype.setDeferAutoSize = function(value) {
  if (!value)
    resumeDeferredAutoSize();
  this.autosizeDeferred = value;
};

/**
 * Allows the element to resize to most recent set of autosize dimensions if
 * autosizing is being deferred.
 */
ExtensionOptionsImpl.prototype.resumeDeferredAutoSize = function() {
  if (this.autosizeDeferred) {
    this.resize(this.deferredAutoSizeState.newWidth,
                this.deferredAutoSizeState.newHeight,
                this.deferredAutoSizeState.oldWidth,
                this.deferredAutoSizeState.oldHeight);
  }
};

GuestViewContainer.registerElement(ExtensionOptionsImpl);
