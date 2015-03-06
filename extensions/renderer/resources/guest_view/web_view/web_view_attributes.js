// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the attributes of the <webview> tag.

var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var WebViewImpl = require('webView').WebViewImpl;
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WebViewInternal = require('webViewInternal').WebViewInternal;

// -----------------------------------------------------------------------------
// Attribute objects.

// Default implementation of a WebView attribute.
function WebViewAttribute(name, webViewImpl) {
  this.name = name;
  this.webViewImpl = webViewImpl;
  this.ignoreMutation = false;

  this.defineProperty();
}

// Retrieves and returns the attribute's value.
WebViewAttribute.prototype.getValue = function() {
  return this.webViewImpl.element.getAttribute(this.name) || '';
};

// Sets the attribute's value.
WebViewAttribute.prototype.setValue = function(value) {
  this.webViewImpl.element.setAttribute(this.name, value || '');
};

// Changes the attribute's value without triggering its mutation handler.
WebViewAttribute.prototype.setValueIgnoreMutation = function(value) {
  this.ignoreMutation = true;
  this.setValue(value);
  this.ignoreMutation = false;
}

// Defines this attribute as a property on the webview node.
WebViewAttribute.prototype.defineProperty = function() {
  Object.defineProperty(this.webViewImpl.element, this.name, {
    get: function() {
      return this.getValue();
    }.bind(this),
    set: function(value) {
      this.setValue(value);
    }.bind(this),
    enumerable: true
  });
};

// Called when the attribute's value changes.
WebViewAttribute.prototype.maybeHandleMutation = function(oldValue, newValue) {
  if (this.ignoreMutation) {
    return;
  }

  this.handleMutation(oldValue, newValue);
};

// Called when a change that isn't ignored occurs to the attribute's value.
WebViewAttribute.prototype.handleMutation = function(oldValue, newValue) {};

// Called when the <webview> element is attached to the DOM tree.
WebViewAttribute.prototype.attach = function() {};

// Called when the <webview> element is detached from the DOM tree.
WebViewAttribute.prototype.detach = function() {};

// An attribute that is treated as a Boolean.
function BooleanAttribute(name, webViewImpl) {
  WebViewAttribute.call(this, name, webViewImpl);
}

BooleanAttribute.prototype.__proto__ = WebViewAttribute.prototype;

BooleanAttribute.prototype.getValue = function() {
  return this.webViewImpl.element.hasAttribute(this.name);
};

BooleanAttribute.prototype.setValue = function(value) {
  if (!value) {
    this.webViewImpl.element.removeAttribute(this.name);
  } else {
    this.webViewImpl.element.setAttribute(this.name, '');
  }
};

// Attribute that specifies whether transparency is allowed in the webview.
function AllowTransparencyAttribute(webViewImpl) {
  BooleanAttribute.call(
      this, WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY, webViewImpl);
}

AllowTransparencyAttribute.prototype.__proto__ = BooleanAttribute.prototype;

AllowTransparencyAttribute.prototype.handleMutation = function(oldValue,
                                                               newValue) {
  if (!this.webViewImpl.guest.getId()) {
    return;
  }

  WebViewInternal.setAllowTransparency(this.webViewImpl.guest.getId(),
                                       this.getValue());
};

// Attribute that specifies whether transparency is allowed in the webview.
function AllowScalingAttribute(webViewImpl) {
  BooleanAttribute.call(
      this, WebViewConstants.ATTRIBUTE_ALLOWSCALING, webViewImpl);
}

AllowScalingAttribute.prototype.__proto__ = BooleanAttribute.prototype;

AllowScalingAttribute.prototype.handleMutation = function(oldValue,
                                                          newValue) {
  if (!this.webViewImpl.guest.getId()) {
    return;
  }

  WebViewInternal.setAllowScaling(this.webViewImpl.guest.getId(),
                                  this.getValue());
};

// Attribute used to define the demension limits of autosizing.
function AutosizeDimensionAttribute(name, webViewImpl) {
  WebViewAttribute.call(this, name, webViewImpl);
}

AutosizeDimensionAttribute.prototype.__proto__ = WebViewAttribute.prototype;

AutosizeDimensionAttribute.prototype.getValue = function() {
  return parseInt(this.webViewImpl.element.getAttribute(this.name)) || 0;
};

AutosizeDimensionAttribute.prototype.handleMutation = function(
    oldValue, newValue) {
  if (!this.webViewImpl.guest.getId()) {
    return;
  }
  this.webViewImpl.guest.setSize({
    'enableAutoSize': this.webViewImpl.attributes[
        WebViewConstants.ATTRIBUTE_AUTOSIZE].getValue(),
    'min': {
      'width': this.webViewImpl.attributes[
          WebViewConstants.ATTRIBUTE_MINWIDTH].getValue(),
      'height': this.webViewImpl.attributes[
          WebViewConstants.ATTRIBUTE_MINHEIGHT].getValue()
    },
    'max': {
      'width': this.webViewImpl.attributes[
          WebViewConstants.ATTRIBUTE_MAXWIDTH].getValue(),
      'height': this.webViewImpl.attributes[
          WebViewConstants.ATTRIBUTE_MAXHEIGHT].getValue()
    }
  });
  return;
};

// Attribute that specifies whether the webview should be autosized.
function AutosizeAttribute(webViewImpl) {
  BooleanAttribute.call(this, WebViewConstants.ATTRIBUTE_AUTOSIZE, webViewImpl);
}

AutosizeAttribute.prototype.__proto__ = BooleanAttribute.prototype;

AutosizeAttribute.prototype.handleMutation =
    AutosizeDimensionAttribute.prototype.handleMutation;

// Attribute that sets the guest content's window.name object.
function NameAttribute(webViewImpl) {
  WebViewAttribute.call(this, WebViewConstants.ATTRIBUTE_NAME, webViewImpl);
}

NameAttribute.prototype.__proto__ = WebViewAttribute.prototype

NameAttribute.prototype.handleMutation = function(oldValue, newValue) {
  oldValue = oldValue || '';
  newValue = newValue || '';
  if (oldValue === newValue || !this.webViewImpl.guest.getId()) {
    return;
  }

  WebViewInternal.setName(this.webViewImpl.guest.getId(), newValue);
};

NameAttribute.prototype.setValue = function(value) {
  value = value || '';
  if (value === '') {
    this.webViewImpl.element.removeAttribute(this.name);
  } else {
    this.webViewImpl.element.setAttribute(this.name, value);
  }
};

// Attribute representing the state of the storage partition.
function PartitionAttribute(webViewImpl) {
  WebViewAttribute.call(
      this, WebViewConstants.ATTRIBUTE_PARTITION, webViewImpl);
  this.validPartitionId = true;
}

PartitionAttribute.prototype.__proto__ = WebViewAttribute.prototype;

PartitionAttribute.prototype.handleMutation = function(oldValue, newValue) {
  newValue = newValue || '';

  // The partition cannot change if the webview has already navigated.
  if (!this.webViewImpl.attributes[
          WebViewConstants.ATTRIBUTE_SRC].beforeFirstNavigation) {
    window.console.error(WebViewConstants.ERROR_MSG_ALREADY_NAVIGATED);
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  if (newValue == 'persist:') {
    this.validPartitionId = false;
    window.console.error(
        WebViewConstants.ERROR_MSG_INVALID_PARTITION_ATTRIBUTE);
  }
};

PartitionAttribute.prototype.detach = function() {
  this.validPartitionId = true;
};

// Attribute that handles the location and navigation of the webview.
function SrcAttribute(webViewImpl) {
  WebViewAttribute.call(this, WebViewConstants.ATTRIBUTE_SRC, webViewImpl);
  this.setupMutationObserver();
  this.beforeFirstNavigation = true;
  this.elementAttached = false;
}

SrcAttribute.prototype.__proto__ = WebViewAttribute.prototype;

SrcAttribute.prototype.setValueIgnoreMutation = function(value) {
  WebViewAttribute.prototype.setValueIgnoreMutation.call(this, value);
  // takeRecords() is needed to clear queued up src mutations. Without it, it is
  // possible for this change to get picked up asyncronously by src's mutation
  // observer |observer|, and then get handled even though we do not want to
  // handle this mutation.
  this.observer.takeRecords();
}

SrcAttribute.prototype.handleMutation = function(oldValue, newValue) {
  // Once we have navigated, we don't allow clearing the src attribute.
  // Once <webview> enters a navigated state, it cannot return to a
  // placeholder state.
  if (!newValue && oldValue) {
    // src attribute changes normally initiate a navigation. We suppress
    // the next src attribute handler call to avoid reloading the page
    // on every guest-initiated navigation.
    this.setValueIgnoreMutation(oldValue);
    return;
  }
  this.parse();
};

SrcAttribute.prototype.attach = function() {
  this.elementAttached = true;
  this.parse();
};

SrcAttribute.prototype.detach = function() {
  this.beforeFirstNavigation = true;
  this.elementAttached = false;
};

// The purpose of this mutation observer is to catch assignment to the src
// attribute without any changes to its value. This is useful in the case
// where the webview guest has crashed and navigating to the same address
// spawns off a new process.
SrcAttribute.prototype.setupMutationObserver =
    function() {
  this.observer = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, function(mutation) {
      var oldValue = mutation.oldValue;
      var newValue = this.getValue();
      if (oldValue != newValue) {
        return;
      }
      this.handleMutation(oldValue, newValue);
    }.bind(this));
  }.bind(this));
  var params = {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: [this.name]
  };
  this.observer.observe(this.webViewImpl.element, params);
};

SrcAttribute.prototype.parse = function() {
  if (!this.elementAttached ||
      !this.webViewImpl.attributes[
          WebViewConstants.ATTRIBUTE_PARTITION].validPartitionId ||
      !this.getValue()) {
    return;
  }

  if (!this.webViewImpl.guest.getId()) {
    if (this.beforeFirstNavigation) {
      this.beforeFirstNavigation = false;
      this.webViewImpl.createGuest();
    }
    return;
  }

  // Navigate to |src|.
  WebViewInternal.navigate(this.webViewImpl.guest.getId(), this.getValue());
};

// -----------------------------------------------------------------------------

// Sets up all of the webview attributes.
WebViewImpl.prototype.setupWebViewAttributes = function() {
  this.attributes = {};

  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY] =
      new AllowTransparencyAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWSCALING] =
      new AllowScalingAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_AUTOSIZE] =
      new AutosizeAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_NAME] =
      new NameAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION] =
      new PartitionAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_SRC] =
      new SrcAttribute(this);

  var autosizeAttributes = [WebViewConstants.ATTRIBUTE_MAXHEIGHT,
                            WebViewConstants.ATTRIBUTE_MAXWIDTH,
                            WebViewConstants.ATTRIBUTE_MINHEIGHT,
                            WebViewConstants.ATTRIBUTE_MINWIDTH];
  for (var i = 0; autosizeAttributes[i]; ++i) {
    this.attributes[autosizeAttributes[i]] =
        new AutosizeDimensionAttribute(autosizeAttributes[i], this);
  }
};
