// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements WebView (<webview>) as a custom element that wraps a
// BrowserPlugin object element. The object element is hidden within
// the shadow DOM of the WebView element.

var GuestView = require('guestView').GuestView;
var GuestViewContainer = require('guestViewContainer').GuestViewContainer;
var GuestViewInternalNatives = requireNative('guest_view_internal');
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;
var WebViewAttributes = require('webViewAttributes').WebViewAttributes;
var WebViewEvents = require('webViewEvents').WebViewEvents;
var WebViewInternal = getInternalApi ?
    getInternalApi('webViewInternal') :
    require('webViewInternal').WebViewInternal;

// Represents the internal state of <webview>.
function WebViewImpl(webviewElement) {
  $Function.call(GuestViewContainer, this, webviewElement, 'webview');
  this.cachedZoom = 1;
  this.setupElementProperties();
  new WebViewEvents(this, this.viewInstanceId);
}

WebViewImpl.prototype.__proto__ = GuestViewContainer.prototype;

// Sets up all of the webview attributes.
WebViewImpl.prototype.setupAttributes = function() {
  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWSCALING] =
      new WebViewAttributes.AllowScalingAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_ALLOWTRANSPARENCY] =
      new WebViewAttributes.AllowTransparencyAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_AUTOSIZE] =
      new WebViewAttributes.AutosizeAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_NAME] =
      new WebViewAttributes.NameAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION] =
      new WebViewAttributes.PartitionAttribute(this);
  this.attributes[WebViewConstants.ATTRIBUTE_SRC] =
      new WebViewAttributes.SrcAttribute(this);

  var autosizeAttributes = [
    WebViewConstants.ATTRIBUTE_MAXHEIGHT, WebViewConstants.ATTRIBUTE_MAXWIDTH,
    WebViewConstants.ATTRIBUTE_MINHEIGHT, WebViewConstants.ATTRIBUTE_MINWIDTH
  ];
  for (var attribute of autosizeAttributes) {
    this.attributes[attribute] =
        new WebViewAttributes.AutosizeDimensionAttribute(attribute, this);
  }
};

// Initiates navigation once the <webview> element is attached to the DOM.
WebViewImpl.prototype.onElementAttached = function() {
  // Mark all attributes as dirty on attachment.
  for (var i in this.attributes) {
    this.attributes[i].dirty = true;
  }
  for (var i in this.attributes) {
    this.attributes[i].attach();
  }
};

// Resets some state upon detaching <webview> element from the DOM.
WebViewImpl.prototype.onElementDetached = function() {
  this.guest.destroy();
  for (var i in this.attributes) {
    this.attributes[i].dirty = false;
  }
  for (var i in this.attributes) {
    this.attributes[i].detach();
  }
};

// Sets the <webview>.request property.
WebViewImpl.prototype.setRequestPropertyOnWebViewElement = function(request) {
  $Object.defineProperty(
      this.element, 'request', {value: request, enumerable: true});
};

WebViewImpl.prototype.setupElementProperties = function() {
  // We cannot use {writable: true} property descriptor because we want a
  // dynamic getter value.
  $Object.defineProperty(this.element, 'contentWindow', {
    get: $Function.bind(
        function() {
          return this.guest.getContentWindow();
        },
        this),
    // No setter.
    enumerable: true
  });
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

WebViewImpl.prototype.createGuest = function() {
  this.guest.create(this.buildParams(), $Function.bind(function() {
    this.attachWindow$();
  }, this));
};

WebViewImpl.prototype.onFrameNameChanged = function(name) {
  this.attributes[WebViewConstants.ATTRIBUTE_NAME].setValueIgnoreMutation(name);
};

// Updates state upon loadcommit.
WebViewImpl.prototype.onLoadCommit = function(
    baseUrlForDataUrl, currentEntryIndex, entryCount,
    processId, url, isTopLevel) {
  this.baseUrlForDataUrl = baseUrlForDataUrl;
  this.currentEntryIndex = currentEntryIndex;
  this.entryCount = entryCount;
  this.processId = processId;
  if (isTopLevel) {
    // Touching the src attribute triggers a navigation. To avoid
    // triggering a page reload on every guest-initiated navigation,
    // we do not handle this mutation.
    this.attributes[
        WebViewConstants.ATTRIBUTE_SRC].setValueIgnoreMutation(url);
  }
};

WebViewImpl.prototype.onAttach = function(storagePartitionId) {
  this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].setValueIgnoreMutation(
      storagePartitionId);
};

WebViewImpl.prototype.buildContainerParams = function() {
  var params = { 'initialZoomFactor': this.cachedZoomFactor,
                 'userAgentOverride': this.userAgentOverride };
  for (var i in this.attributes) {
    var value = this.attributes[i].getValueIfDirty();
    if (value)
      params[i] = value;
  }
  return params;
};

WebViewImpl.prototype.attachWindow$ = function(opt_guestInstanceId) {
  // If |opt_guestInstanceId| was provided, then a different existing guest is
  // being attached to this webview, and the current one will get destroyed.
  if (opt_guestInstanceId) {
    if (this.guest.getId() == opt_guestInstanceId) {
      return true;
    }
    this.guest.destroy();
    this.guest = new GuestView('webview', opt_guestInstanceId);
    this.prepareForReattach_();
  }

  return $Function.call(GuestViewContainer.prototype.attachWindow$, this);
};

WebViewImpl.prototype.addContentScripts = function(rules) {
  return WebViewInternal.addContentScripts(this.viewInstanceId, rules);
};

WebViewImpl.prototype.removeContentScripts = function(names) {
  return WebViewInternal.removeContentScripts(this.viewInstanceId, names);
};

// Shared implementation of executeScript() and insertCSS().
WebViewImpl.prototype.executeCode = function(func, args) {
  if (!this.guest.getId()) {
    window.console.error(WebViewConstants.ERROR_MSG_CANNOT_INJECT_SCRIPT);
    return false;
  }

  var webviewSrc = this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue();
  if (this.baseUrlForDataUrl) {
    webviewSrc = this.baseUrlForDataUrl;
  }

  args = $Array.concat([this.guest.getId(), webviewSrc],
                       $Array.slice(args));
  $Function.apply(func, null, args);
  return true;
};

WebViewImpl.prototype.executeScript = function(var_args) {
  return this.executeCode(
      WebViewInternal.executeScript, $Array.slice(arguments));
};

WebViewImpl.prototype.insertCSS = function(var_args) {
  return this.executeCode(WebViewInternal.insertCSS, $Array.slice(arguments));
};

WebViewImpl.prototype.back = function(callback) {
  return this.go(-1, callback);
};

WebViewImpl.prototype.canGoBack = function() {
  return this.entryCount > 1 && this.currentEntryIndex > 0;
};

WebViewImpl.prototype.canGoForward = function() {
  return this.currentEntryIndex >= 0 &&
      this.currentEntryIndex < (this.entryCount - 1);
};

WebViewImpl.prototype.forward = function(callback) {
  return this.go(1, callback);
};

WebViewImpl.prototype.getProcessId = function() {
  return this.processId;
};

WebViewImpl.prototype.getUserAgent = function() {
  return this.userAgentOverride || navigator.userAgent;
};

WebViewImpl.prototype.isUserAgentOverridden = function() {
  return !!this.userAgentOverride &&
      this.userAgentOverride != navigator.userAgent;
};

WebViewImpl.prototype.setUserAgentOverride = function(userAgentOverride) {
  this.userAgentOverride = userAgentOverride;
  if (!this.guest.getId()) {
    // If we are not attached yet, then we will pick up the user agent on
    // attachment.
    return false;
  }
  WebViewInternal.overrideUserAgent(this.guest.getId(), userAgentOverride);
  return true;
};

WebViewImpl.prototype.loadDataWithBaseUrl = function(
    dataUrl, baseUrl, virtualUrl) {
  if (!this.guest.getId()) {
    return;
  }
  WebViewInternal.loadDataWithBaseUrl(
      this.guest.getId(), dataUrl, baseUrl, virtualUrl, function() {
        // Report any errors.
        if (chrome.runtime.lastError != undefined) {
          window.console.error(
              'Error while running webview.loadDataWithBaseUrl: ' +
              chrome.runtime.lastError.message);
        }
      });
};

WebViewImpl.prototype.print = function() {
  return this.executeScript({code: 'window.print();'});
};

WebViewImpl.prototype.setZoom = function(zoomFactor, callback) {
  if (!this.guest.getId()) {
    this.cachedZoomFactor = zoomFactor;
    return false;
  }
  this.cachedZoomFactor = 1;
  WebViewInternal.setZoom(this.guest.getId(), zoomFactor, callback);
  return true;
};

// Requests the <webview> element wihtin the embedder to enter fullscreen.
WebViewImpl.prototype.makeElementFullscreen = function() {
  GuestViewInternalNatives.RunWithGesture($Function.bind(function() {
    this.element.webkitRequestFullScreen();
  }, this));
};

// Implemented when the ChromeWebView API is available.
WebViewImpl.prototype.maybeSetupContextMenus = function() {};

(() => {
  // For <webview> API methods which aren't explicitly defined on |WebViewImpl|,
  // create default implementations which forward the call to |WebViewInternal|.
  var createDefaultApiMethod = function(m) {
    return function(var_args) {
      if (!this.guest.getId()) {
        return false;
      }
      var args = $Array.concat([this.guest.getId()], $Array.slice(arguments));
      $Function.apply(WebViewInternal[m], null, args);
      return true;
    };
  };

  for (var methodName of WEB_VIEW_API_METHODS) {
    if (WebViewImpl.prototype[methodName] == undefined) {
      WebViewImpl.prototype[methodName] = createDefaultApiMethod(methodName);
    }
  }
})();

// Exports.
exports.$set('WebViewImpl', WebViewImpl);
