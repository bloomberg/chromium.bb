// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements WebView (<webview>) as a custom element that wraps a
// BrowserPlugin object element. The object element is hidden within
// the shadow DOM of the WebView element.

var DocumentNatives = requireNative('document_natives');
var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var GuestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WebViewEvents = require('webViewEvents').WebViewEvents;
var WebViewInternal = require('webViewInternal').WebViewInternal;

// Represents the internal state of <webview>.
function WebViewImpl(webviewElement) {
  GuestViewContainer.call(this, webviewElement)

  this.guest = new GuestView('webview');
  this.beforeFirstNavigation = true;
  this.viewInstanceId = IdGenerator.GetNextId();

  this.setupWebViewAttributes();
  this.setupFocusPropagation();
  this.setupElementProperties();

  // on* Event handlers.
  this.on = {};
  new WebViewEvents(this, this.viewInstanceId);

  var shadowRoot = this.element.createShadowRoot();
  shadowRoot.appendChild(this.browserPluginElement);
}

WebViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

WebViewImpl.VIEW_TYPE = 'WebView';

// Add extra functionality to |this.element|.
WebViewImpl.setupElement = function(proto) {
  // Public-facing API methods.
  var apiMethods = WebViewImpl.getApiMethods();

  // Add the experimental API methods, if available.
  var experimentalApiMethods =
      WebViewImpl.maybeGetExperimentalApiMethods();
  apiMethods = $Array.concat(apiMethods, experimentalApiMethods);

  // Create default implementations for undefined API methods.
  var createDefaultApiMethod = function(m) {
    return function(var_args) {
      if (!this.guest.getId()) {window.console.log(this);
        return false;
      }
      var args = $Array.concat([this.guest.getId()], $Array.slice(arguments));
      $Function.apply(WebViewInternal[m], null, args);
      return true;
    };
  };
  for (var i = 0; i != apiMethods.length; ++i) {
    if (WebViewImpl.prototype[apiMethods[i]] == undefined) {
      WebViewImpl.prototype[apiMethods[i]] =
          createDefaultApiMethod(apiMethods[i]);
    }
  }

  // Forward proto.foo* method calls to WebViewImpl.foo*.
  GuestViewContainer.forwardApiMethods(proto, apiMethods);
};

// Initiates navigation once the <webview> element is attached to the DOM.
WebViewImpl.prototype.onElementAttached = function() {
  this.parseSrcAttribute();
};

// Resets some state upon detaching <webview> element from the DOM.
WebViewImpl.prototype.onElementDetached = function() {
  // If the guest's ID is defined then the <webview> has navigated and has
  // already picked up a partition ID. Thus, we need to reset the initialization
  // state. However, it may be the case that beforeFirstNavigation is false BUT
  // the guest's ID has yet to be initialized. This means that we have not
  // heard back from createGuest yet. We will not reset the flag in this case so
  // that we don't end up allocating a second guest.
  if (this.guest.getId()) {
    this.guest.destroy();
    this.guest = new GuestView('webview');
    this.beforeFirstNavigation = true;
    this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].validPartitionId =
        true;
  }
  this.internalInstanceId = 0;
};

// Sets the <webview>.request property.
WebViewImpl.prototype.setRequestPropertyOnWebViewElement = function(request) {
  Object.defineProperty(
      this.element,
      'request',
      {
        value: request,
        enumerable: true
      }
  );
};

WebViewImpl.prototype.setupFocusPropagation = function() {
  if (!this.element.hasAttribute('tabIndex')) {
    // <webview> needs a tabIndex in order to be focusable.
    // TODO(fsamuel): It would be nice to avoid exposing a tabIndex attribute
    // to allow <webview> to be focusable.
    // See http://crbug.com/231664.
    this.element.setAttribute('tabIndex', -1);
  }
  this.element.addEventListener('focus', function(e) {
    // Focus the BrowserPlugin when the <webview> takes focus.
    this.browserPluginElement.focus();
  }.bind(this));
  this.element.addEventListener('blur', function(e) {
    // Blur the BrowserPlugin when the <webview> loses focus.
    this.browserPluginElement.blur();
  }.bind(this));
};

WebViewImpl.prototype.setupElementProperties = function() {
  // We cannot use {writable: true} property descriptor because we want a
  // dynamic getter value.
  Object.defineProperty(this.element, 'contentWindow', {
    get: function() {
      if (this.guest.getContentWindow()) {
        return this.guest.getContentWindow();
      }
      window.console.error(
          WebViewConstants.ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE);
    }.bind(this),
    // No setter.
    enumerable: true
  });
};

// This observer monitors mutations to attributes of the <webview>.
WebViewImpl.prototype.handleAttributeMutation = function(
    attributeName, oldValue, newValue) {
  if (!this.attributes[attributeName] ||
      this.attributes[attributeName].ignoreMutation) {
    return;
  }

  // Let the changed attribute handle its own mutation;
  this.attributes[attributeName].handleMutation(oldValue, newValue);
};

WebViewImpl.prototype.handleBrowserPluginAttributeMutation = function(
    attributeName, oldValue, newValue) {
  if (attributeName == WebViewConstants.ATTRIBUTE_INTERNALINSTANCEID &&
      !oldValue && !!newValue) {
    this.browserPluginElement.removeAttribute(
        WebViewConstants.ATTRIBUTE_INTERNALINSTANCEID);
    this.internalInstanceId = parseInt(newValue);

    if (!this.guest.getId()) {
      return;
    }
    this.guest.attach(this.internalInstanceId, this.buildAttachParams());
  }
};

WebViewImpl.prototype.onSizeChanged = function(webViewEvent) {
  var newWidth = webViewEvent.newWidth;
  var newHeight = webViewEvent.newHeight;

  var element = this.element;

  var width = element.offsetWidth;
  var height = element.offsetHeight;

  // Check the current bounds to make sure we do not resize <webview>
  // outside of current constraints.
  var maxWidth = this.attributes[
    WebViewConstants.ATTRIBUTE_MAXWIDTH].getValue() || width;
  var minWidth = this.attributes[
    WebViewConstants.ATTRIBUTE_MINWIDTH].getValue() || width;
  var maxHeight = this.attributes[
    WebViewConstants.ATTRIBUTE_MAXHEIGHT].getValue() || height;
  var minHeight = this.attributes[
    WebViewConstants.ATTRIBUTE_MINHEIGHT].getValue() || height;

  minWidth = Math.min(minWidth, maxWidth);
  minHeight = Math.min(minHeight, maxHeight);

  if (!this.attributes[WebViewConstants.ATTRIBUTE_AUTOSIZE].getValue() ||
      (newWidth >= minWidth &&
      newWidth <= maxWidth &&
      newHeight >= minHeight &&
      newHeight <= maxHeight)) {
    element.style.width = newWidth + 'px';
    element.style.height = newHeight + 'px';
    // Only fire the DOM event if the size of the <webview> has actually
    // changed.
    this.dispatchEvent(webViewEvent);
  }
};

WebViewImpl.prototype.parseSrcAttribute = function() {
  if (!this.elementAttached ||
      !this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].validPartitionId ||
      !this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue()) {
    return;
  }

  if (!this.guest.getId()) {
    if (this.beforeFirstNavigation) {
      this.beforeFirstNavigation = false;
      this.createGuest();
    }
    return;
  }

  // Navigate to |this.src|.
  WebViewInternal.navigate(
      this.guest.getId(),
      this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue());
};

WebViewImpl.prototype.createGuest = function() {
  var params = {
    'storagePartitionId': this.attributes[
      WebViewConstants.ATTRIBUTE_PARTITION].getValue()
  };

  this.guest.create(params,
                    function() {
                      this.attachWindow();
                    }.bind(this));
};

WebViewImpl.prototype.onFrameNameChanged = function(name) {
  name = name || '';
  if (name === '') {
    this.element.removeAttribute(WebViewConstants.ATTRIBUTE_NAME);
  } else {
    this.attributes[WebViewConstants.ATTRIBUTE_NAME].setValue(name);
  }
};

WebViewImpl.prototype.dispatchEvent = function(webViewEvent) {
  return this.element.dispatchEvent(webViewEvent);
};

// Adds an 'on<event>' property on the webview, which can be used to set/unset
// an event handler.
WebViewImpl.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  Object.defineProperty(this.element, propertyName, {
    get: function() {
      return this.on[propertyName];
    }.bind(this),
    set: function(value) {
      if (this.on[propertyName])
        this.element.removeEventListener(eventName, this.on[propertyName]);
      this.on[propertyName] = value;
      if (value)
        this.element.addEventListener(eventName, value);
    }.bind(this),
    enumerable: true
  });
};

// Updates state upon loadcommit.
WebViewImpl.prototype.onLoadCommit = function(
    baseUrlForDataUrl, currentEntryIndex, entryCount,
    processId, url, isTopLevel) {
  this.baseUrlForDataUrl = baseUrlForDataUrl;
  this.currentEntryIndex = currentEntryIndex;
  this.entryCount = entryCount;
  this.processId = processId;
  var oldValue = this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue();
  var newValue = url;
  if (isTopLevel && (oldValue != newValue)) {
    // Touching the src attribute triggers a navigation. To avoid
    // triggering a page reload on every guest-initiated navigation,
    // we do not handle this mutation.
    this.attributes[WebViewConstants.ATTRIBUTE_SRC].setValueIgnoreMutation(
        newValue);
  }
};

WebViewImpl.prototype.onAttach = function(storagePartitionId) {
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].setValue(
      storagePartitionId);
};

WebViewImpl.prototype.buildAttachParams = function() {
  var params = {
    'instanceId': this.viewInstanceId,
    'userAgentOverride': this.userAgentOverride
  };
  for (var i in this.attributes) {
    params[i] = this.attributes[i].getValue();
  }
  return params;
};

WebViewImpl.prototype.attachWindow = function(guestInstanceId) {
  if (guestInstanceId) {
    if (this.guest.getId() && this.guest.getId() != guestInstanceId) {
      this.guest.destroy();
    }
    this.guest = new GuestView('webview', guestInstanceId);
  }

  if (!this.internalInstanceId) {
    return true;
  }

  this.guest.attach(this.internalInstanceId, this.buildAttachParams());
  return true;
};

// Shared implementation of executeScript() and insertCSS().
WebViewImpl.prototype.executeCode = function(func, args) {
  if (!this.guest.getId()) {
    window.console.error(WebViewConstants.ERROR_MSG_CANNOT_INJECT_SCRIPT);
    return false;
  }

  var webviewSrc = this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue();
  if (this.baseUrlForDataUrl != '') {
    webviewSrc = this.baseUrlForDataUrl;
  }

  args = $Array.concat([this.guest.getId(), webviewSrc],
                       $Array.slice(args));
  $Function.apply(func, null, args);
  return true;
}

// Implemented when the ChromeWebView API is available.
WebViewImpl.prototype.maybeGetChromeWebViewEvents = function() {};

// Implemented when the experimental WebView API is available.
WebViewImpl.maybeGetExperimentalApiMethods = function() { return []; };
WebViewImpl.prototype.setupExperimentalContextMenus = function() {};

GuestViewContainer.listenForReadyStateChange(WebViewImpl);

// Exports.
exports.WebViewImpl = WebViewImpl;
