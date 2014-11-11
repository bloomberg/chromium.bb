// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements WebView (<webview>) as a custom element that wraps a
// BrowserPlugin object element. The object element is hidden within
// the shadow DOM of the WebView element.

var DocumentNatives = requireNative('document_natives');
var GuestViewInternal =
    require('binding').Binding.create('guestViewInternal').generate();
var guestViewInternalNatives = requireNative('guest_view_internal');
var IdGenerator = requireNative('id_generator');
var WebViewConstants = require('webViewConstants').WebViewConstants;
var WebViewEvents = require('webViewEvents').WebViewEvents;
var WebViewInternal = require('webViewInternal').WebViewInternal;

// Represents the internal state of the WebView node.
function WebViewImpl(webviewNode) {
  privates(webviewNode).internal = this;
  this.webviewNode = webviewNode;
  this.attached = false;
  this.pendingGuestCreation = false;
  this.elementAttached = false;

  this.beforeFirstNavigation = true;
  this.contentWindow = null;

  // on* Event handlers.
  this.on = {};

  this.browserPluginNode = this.createBrowserPluginNode();
  var shadowRoot = this.webviewNode.createShadowRoot();
  this.setupWebViewAttributes();
  this.setupFocusPropagation();
  this.setupWebviewNodeProperties();

  this.viewInstanceId = IdGenerator.GetNextId();

  new WebViewEvents(this, this.viewInstanceId);

  shadowRoot.appendChild(this.browserPluginNode);
}

WebViewImpl.prototype.createBrowserPluginNode = function() {
  // We create BrowserPlugin as a custom element in order to observe changes
  // to attributes synchronously.
  var browserPluginNode = new WebViewImpl.BrowserPlugin();
  privates(browserPluginNode).internal = this;
  return browserPluginNode;
};

// Resets some state upon reattaching <webview> element to the DOM.
WebViewImpl.prototype.reset = function() {
  // If guestInstanceId is defined then the <webview> has navigated and has
  // already picked up a partition ID. Thus, we need to reset the initialization
  // state. However, it may be the case that beforeFirstNavigation is false BUT
  // guestInstanceId has yet to be initialized. This means that we have not
  // heard back from createGuest yet. We will not reset the flag in this case so
  // that we don't end up allocating a second guest.
  if (this.guestInstanceId) {
    GuestViewInternal.destroyGuest(this.guestInstanceId);
    this.guestInstanceId = undefined;
    this.beforeFirstNavigation = true;
    this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].validPartitionId =
        true;
    this.contentWindow = null;
  }
  this.internalInstanceId = 0;
};

// Sets the <webview>.request property.
WebViewImpl.prototype.setRequestPropertyOnWebViewNode = function(request) {
  Object.defineProperty(
      this.webviewNode,
      'request',
      {
        value: request,
        enumerable: true
      }
  );
};

WebViewImpl.prototype.setupFocusPropagation = function() {
  if (!this.webviewNode.hasAttribute('tabIndex')) {
    // <webview> needs a tabIndex in order to be focusable.
    // TODO(fsamuel): It would be nice to avoid exposing a tabIndex attribute
    // to allow <webview> to be focusable.
    // See http://crbug.com/231664.
    this.webviewNode.setAttribute('tabIndex', -1);
  }
  this.webviewNode.addEventListener('focus', function(e) {
    // Focus the BrowserPlugin when the <webview> takes focus.
    this.browserPluginNode.focus();
  }.bind(this));
  this.webviewNode.addEventListener('blur', function(e) {
    // Blur the BrowserPlugin when the <webview> loses focus.
    this.browserPluginNode.blur();
  }.bind(this));
};

WebViewImpl.prototype.setupWebviewNodeProperties = function() {
  // We cannot use {writable: true} property descriptor because we want a
  // dynamic getter value.
  Object.defineProperty(this.webviewNode, 'contentWindow', {
    get: function() {
      if (this.contentWindow) {
        return this.contentWindow;
      }
      window.console.error(
          WebViewConstants.ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE);
    }.bind(this),
    // No setter.
    enumerable: true
  });
};

// This observer monitors mutations to attributes of the <webview> and
// updates the BrowserPlugin properties accordingly. In turn, updating
// a BrowserPlugin property will update the corresponding BrowserPlugin
// attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
// details.
WebViewImpl.prototype.handleWebviewAttributeMutation = function(
    attributeName, oldValue, newValue) {
  if (!this.attributes[attributeName] ||
      this.attributes[attributeName].ignoreMutation) {
    return;
  }

  // Let the changed attribute handle its own mutation;
  this.attributes[attributeName].handleMutation(oldValue, newValue);
};

WebViewImpl.prototype.handleBrowserPluginAttributeMutation =
    function(attributeName, oldValue, newValue) {
  if (attributeName == WebViewConstants.ATTRIBUTE_INTERNALINSTANCEID &&
      !oldValue && !!newValue) {
    this.browserPluginNode.removeAttribute(
        WebViewConstants.ATTRIBUTE_INTERNALINSTANCEID);
    this.internalInstanceId = parseInt(newValue);

    if (!this.guestInstanceId) {
      return;
    }
    guestViewInternalNatives.AttachGuest(
        this.internalInstanceId,
        this.guestInstanceId,
        this.buildAttachParams(),
        function(w) {
          this.contentWindow = w;
        }.bind(this)
    );
  }
};

WebViewImpl.prototype.onSizeChanged = function(webViewEvent) {
  var newWidth = webViewEvent.newWidth;
  var newHeight = webViewEvent.newHeight;

  var node = this.webviewNode;

  var width = node.offsetWidth;
  var height = node.offsetHeight;

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
    node.style.width = newWidth + 'px';
    node.style.height = newHeight + 'px';
    // Only fire the DOM event if the size of the <webview> has actually
    // changed.
    this.dispatchEvent(webViewEvent);
  }
};

WebViewImpl.prototype.parseSrcAttribute = function() {
  if (!this.attributes[WebViewConstants.ATTRIBUTE_PARTITION].validPartitionId ||
      !this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue()) {
    return;
  }

  if (this.guestInstanceId == undefined) {
    if (this.beforeFirstNavigation) {
      this.beforeFirstNavigation = false;
      this.createGuest();
    }
    return;
  }

  // Navigate to |this.src|.
  WebViewInternal.navigate(
      this.guestInstanceId,
      this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue());
};

WebViewImpl.prototype.createGuest = function() {
  if (this.pendingGuestCreation) {
    return;
  }
  var params = {
    'storagePartitionId': this.attributes[
        WebViewConstants.ATTRIBUTE_PARTITION].getValue()
  };
  GuestViewInternal.createGuest(
      'webview',
      params,
      function(guestInstanceId) {
        this.pendingGuestCreation = false;
        if (!this.elementAttached) {
          GuestViewInternal.destroyGuest(guestInstanceId);
          return;
        }
        this.attachWindow(guestInstanceId);
      }.bind(this)
  );
  this.pendingGuestCreation = true;
};

WebViewImpl.prototype.onFrameNameChanged = function(name) {
  name = name || '';
  if (name === '') {
    this.webviewNode.removeAttribute(WebViewConstants.ATTRIBUTE_NAME);
  } else {
    this.attributes[WebViewConstants.ATTRIBUTE_NAME].setValue(name);
  }
};

WebViewImpl.prototype.dispatchEvent = function(webViewEvent) {
  return this.webviewNode.dispatchEvent(webViewEvent);
};

// Adds an 'on<event>' property on the webview, which can be used to set/unset
// an event handler.
WebViewImpl.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  Object.defineProperty(this.webviewNode, propertyName, {
    get: function() {
      return this.on[propertyName];
    }.bind(this),
    set: function(value) {
      if (this.on[propertyName])
        this.webviewNode.removeEventListener(eventName, this.on[propertyName]);
      this.on[propertyName] = value;
      if (value)
        this.webviewNode.addEventListener(eventName, value);
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
  this.guestInstanceId = guestInstanceId;
  var params = this.buildAttachParams();

  if (!this.internalInstanceId) {
    return true;
  }

  return guestViewInternalNatives.AttachGuest(
      this.internalInstanceId,
      this.guestInstanceId,
      params, function(w) {
        this.contentWindow = w;
      }.bind(this)
  );
};

// -----------------------------------------------------------------------------
// Public-facing API methods.


// Navigates to the previous history entry.
WebViewImpl.prototype.back = function(callback) {
  return this.go(-1, callback);
};

// Returns whether there is a previous history entry to navigate to.
WebViewImpl.prototype.canGoBack = function() {
  return this.entryCount > 1 && this.currentEntryIndex > 0;
};

// Returns whether there is a subsequent history entry to navigate to.
WebViewImpl.prototype.canGoForward = function() {
  return this.currentEntryIndex >= 0 &&
      this.currentEntryIndex < (this.entryCount - 1);
};

// Clears browsing data for the WebView partition.
WebViewImpl.prototype.clearData = function() {
  if (!this.guestInstanceId) {
    return;
  }
  var args = $Array.concat([this.guestInstanceId], $Array.slice(arguments));
  $Function.apply(WebViewInternal.clearData, null, args);
};

// Shared implementation of executeScript() and insertCSS().
WebViewImpl.prototype.executeCode = function(func, args) {
  if (!this.guestInstanceId) {
    window.console.error(WebViewConstants.ERROR_MSG_CANNOT_INJECT_SCRIPT);
    return;
  }

  var webviewSrc = this.attributes[WebViewConstants.ATTRIBUTE_SRC].getValue();
  if (this.baseUrlForDataUrl != '') {
    webviewSrc = this.baseUrlForDataUrl;
  }

  args = $Array.concat([this.guestInstanceId, webviewSrc],
                       $Array.slice(args));
  $Function.apply(func, null, args);
}

// Injects JavaScript code into the guest page.
WebViewImpl.prototype.executeScript = function(var_args) {
  this.executeCode(WebViewInternal.executeScript, $Array.slice(arguments));
};

// Initiates a find-in-page request.
WebViewImpl.prototype.find = function(search_text, options, callback) {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.find(this.guestInstanceId, search_text, options, callback);
};

// Navigates to the subsequent history entry.
WebViewImpl.prototype.forward = function(callback) {
  return this.go(1, callback);
};

// Returns Chrome's internal process ID for the guest web page's current
// process.
WebViewImpl.prototype.getProcessId = function() {
  return this.processId;
};

// Returns the user agent string used by the webview for guest page requests.
WebViewImpl.prototype.getUserAgent = function() {
  return this.userAgentOverride || navigator.userAgent;
};

// Gets the current zoom factor.
WebViewImpl.prototype.getZoom = function(callback) {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.getZoom(this.guestInstanceId, callback);
};

// Navigates to a history entry using a history index relative to the current
// navigation.
WebViewImpl.prototype.go = function(relativeIndex, callback) {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.go(this.guestInstanceId, relativeIndex, callback);
};

// Injects CSS into the guest page.
WebViewImpl.prototype.insertCSS = function(var_args) {
  this.executeCode(WebViewInternal.insertCSS, $Array.slice(arguments));
};

// Indicates whether or not the webview's user agent string has been overridden.
WebViewImpl.prototype.isUserAgentOverridden = function() {
  return !!this.userAgentOverride &&
      this.userAgentOverride != navigator.userAgent;
};

// Prints the contents of the webview.
WebViewImpl.prototype.print = function() {
  this.executeScript({code: 'window.print();'});
};

// Reloads the current top-level page.
WebViewImpl.prototype.reload = function() {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.reload(this.guestInstanceId);
};

// Override the user agent string used by the webview for guest page requests.
WebViewImpl.prototype.setUserAgentOverride = function(userAgentOverride) {
  this.userAgentOverride = userAgentOverride;
  if (!this.guestInstanceId) {
    // If we are not attached yet, then we will pick up the user agent on
    // attachment.
    return;
  }
  WebViewInternal.overrideUserAgent(this.guestInstanceId, userAgentOverride);
};

// Changes the zoom factor of the page.
WebViewImpl.prototype.setZoom = function(zoomFactor, callback) {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.setZoom(this.guestInstanceId, zoomFactor, callback);
};

// Stops loading the current navigation if one is in progress.
WebViewImpl.prototype.stop = function() {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.stop(this.guestInstanceId);
};

// Ends the current find session.
WebViewImpl.prototype.stopFinding = function(action) {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.stopFinding(this.guestInstanceId, action);
};

// Forcibly kills the guest web page's renderer process.
WebViewImpl.prototype.terminate = function() {
  if (!this.guestInstanceId) {
    return;
  }
  WebViewInternal.terminate(this.guestInstanceId);
};

// -----------------------------------------------------------------------------

// Registers browser plugin <object> custom element.
function registerBrowserPluginElement() {
  var proto = Object.create(HTMLObjectElement.prototype);

  proto.createdCallback = function() {
    this.setAttribute('type', 'application/browser-plugin');
    this.setAttribute('id', 'browser-plugin-' + IdGenerator.GetNextId());
    // The <object> node fills in the <webview> container.
    this.style.width = '100%';
    this.style.height = '100%';
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.handleBrowserPluginAttributeMutation(name, oldValue, newValue);
  };

  proto.attachedCallback = function() {
    // Load the plugin immediately.
    var unused = this.nonExistentAttribute;
  };

  WebViewImpl.BrowserPlugin =
      DocumentNatives.RegisterElement('browserplugin', {extends: 'object',
                                                        prototype: proto});

  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;
}

// Registers <webview> custom element.
function registerWebViewElement() {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    new WebViewImpl(this);
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.handleWebviewAttributeMutation(name, oldValue, newValue);
  };

  proto.detachedCallback = function() {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    internal.elementAttached = false;
    internal.reset();
  };

  proto.attachedCallback = function() {
    var internal = privates(this).internal;
    if (!internal) {
      return;
    }
    if (!internal.elementAttached) {
      internal.elementAttached = true;
      internal.parseSrcAttribute();
    }
  };

  // Public-facing API methods.
  var methods = [
    'back',
    'canGoBack',
    'canGoForward',
    'clearData',
    'executeScript',
    'find',
    'forward',
    'getProcessId',
    'getUserAgent',
    'getZoom',
    'go',
    'insertCSS',
    'isUserAgentOverridden',
    'print',
    'reload',
    'setUserAgentOverride',
    'setZoom',
    'stop',
    'stopFinding',
    'terminate'
  ];

  // Add the experimental API methods, if available.
  var experimentalMethods =
      WebViewImpl.maybeGetExperimentalAPIs();
  methods = $Array.concat(methods, experimentalMethods);

  // Forward proto.foo* method calls to WebViewImpl.foo*.
  var createHandler = function(m) {
    return function(var_args) {
      var internal = privates(this).internal;
      return $Function.apply(internal[m], internal, arguments);
    };
  };
  for (var i = 0; methods[i]; ++i) {
    proto[methods[i]] = createHandler(methods[i]);
  }

  window.WebView =
      DocumentNatives.RegisterElement('webview', {prototype: proto});

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
  registerWebViewElement();
  window.removeEventListener(event.type, listener, useCapture);
}, useCapture);

// Implemented when the ChromeWebView API is available.
WebViewImpl.prototype.maybeGetChromeWebViewEvents = function() {};

// Implemented when the experimental WebView API is available.
WebViewImpl.maybeGetExperimentalAPIs = function() {};
WebViewImpl.prototype.setupExperimentalContextMenus = function() {};

// Exports.
exports.WebViewImpl = WebViewImpl;
exports.WebViewInternal = WebViewInternal;
