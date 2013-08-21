// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

'use strict';

var eventBindings = require('event_bindings');
var messagingNatives = requireNative('messaging_natives');
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var webRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');

// This secret enables hiding <webview> private members from the outside scope.
// Outside of this file, |secret| is inaccessible. The only way to access the
// <webview> element's internal members is via the |secret|. Since it's only
// accessible by code here (and in web_view_experimental), only <webview>'s
// API can access it and not external developers.
var secret = {};

/** @type {Array.<string>} */
var WEB_VIEW_ATTRIBUTES = ['name', 'src', 'partition', 'autosize', 'minheight',
    'minwidth', 'maxheight', 'maxwidth'];

var webViewInstanceIdCounter = 0;

var createEvent = function(name) {
  var eventOpts = {supportsListeners: true, supportsFilters: true};
  return new eventBindings.Event(name, undefined, eventOpts);
};

var WEB_VIEW_EXT_EVENTS = {
  'close': {
    evt: createEvent('webview.onClose'),
    fields: []
  },
  'consolemessage': {
    evt: createEvent('webview.onConsoleMessage'),
    fields: ['level', 'message', 'line', 'sourceId']
  },
  'contentload': {
    evt: createEvent('webview.onContentLoad'),
    fields: []
  },
  'exit': {
     evt: createEvent('webview.onExit'),
     fields: ['processId', 'reason']
  },
  'loadabort': {
    evt: createEvent('webview.onLoadAbort'),
    fields: ['url', 'isTopLevel', 'reason']
  },
  'loadcommit': {
    customHandler: function(webview, event, webviewEvent) {
      webview.currentEntryIndex_ = event.currentEntryIndex;
      webview.entryCount_ = event.entryCount;
      webview.processId_ = event.processId;
      if (event.isTopLevel) {
        webview.browserPluginNode_.setAttribute('src', event.url);
      }
      webview.webviewNode_.dispatchEvent(webviewEvent);
    },
    evt: createEvent('webview.onLoadCommit'),
    fields: ['url', 'isTopLevel']
  },
  'loadredirect': {
    evt: createEvent('webview.onLoadRedirect'),
    fields: ['isTopLevel', 'oldUrl', 'newUrl']
  },
  'loadstart': {
    evt: createEvent('webview.onLoadStart'),
    fields: ['url', 'isTopLevel']
  },
  'loadstop': {
    evt: createEvent('webview.onLoadStop'),
    fields: []
  },
  'newwindow': {
    cancelable: true,
    customHandler: function(webview, event, webviewEvent) {
      webview.setupExtNewWindowEvent_(event, webviewEvent);
    },
    evt: createEvent('webview.onNewWindow'),
    fields: [
      'initialHeight',
      'initialWidth',
      'targetUrl',
      'windowOpenDisposition',
      'name'
    ]
  },
  'permissionrequest': {
    cancelable: true,
    customHandler: function(webview, event, webviewEvent) {
      webview.setupExtPermissionEvent_(event, webviewEvent);
    },
    evt: createEvent('webview.onPermissionRequest'),
    fields: [
      'lastUnlockedBySelf',
      'permission',
      'requestMethod',
      'url',
      'userGesture'
    ]
  },
  'responsive': {
    evt: createEvent('webview.onResponsive'),
    fields: ['processId']
  },
  'sizechanged': {
    evt: createEvent('webview.onSizeChanged'),
    fields: ['oldHeight', 'oldWidth', 'newHeight', 'newWidth']
  },
  'unresponsive': {
    evt: createEvent('webview.onUnresponsive'),
    fields: ['processId']
  }
};

// Implemented when the experimental API is available.
WebViewInternal.maybeRegisterExperimentalAPIs = function(proto) {}

/**
 * @constructor
 */
function WebViewInternal(webviewNode) {
  this.webviewNode_ = webviewNode;
  this.browserPluginNode_ = this.createBrowserPluginNode_();
  var shadowRoot = this.webviewNode_.webkitCreateShadowRoot();
  shadowRoot.appendChild(this.browserPluginNode_);

  this.setupWebviewNodeAttributes_();
  this.setupFocusPropagation_();
  this.setupWebviewNodeProperties_();
  this.setupWebviewNodeEvents_();
}

/**
 * @private
 */
WebViewInternal.prototype.createBrowserPluginNode_ = function() {
  // We create BrowserPlugin as a custom element in order to observe changes
  // to attributes synchronously.
  var browserPluginNode = new WebViewInternal.BrowserPlugin();
  Object.defineProperty(browserPluginNode, 'internal_', {
    enumerable: false,
    writable: false,
    value: function(key) {
      if (key !== secret) {
        return null;
      }
      return this;
    }.bind(this)
  });

  $Array.forEach(WEB_VIEW_ATTRIBUTES, function(attributeName) {
    // Only copy attributes that have been assigned values, rather than copying
    // a series of undefined attributes to BrowserPlugin.
    if (this.webviewNode_.hasAttribute(attributeName)) {
      browserPluginNode.setAttribute(
        attributeName, this.webviewNode_.getAttribute(attributeName));
    } else if (this.webviewNode_[attributeName]){
      // Reading property using has/getAttribute does not work on
      // document.DOMContentLoaded event (but works on
      // window.DOMContentLoaded event).
      // So copy from property if copying from attribute fails.
      browserPluginNode.setAttribute(
        attributeName, this.webviewNode_[attributeName]);
    }
  }, this);

  return browserPluginNode;
};

/**
 * @private
 */
WebViewInternal.prototype.setupFocusPropagation_ = function() {
  if (!this.webviewNode_.hasAttribute('tabIndex')) {
    // <webview> needs a tabIndex in order to respond to keyboard focus.
    // TODO(fsamuel): This introduces unexpected tab ordering. We need to find
    // a way to take keyboard focus without messing with tab ordering.
    // See http://crbug.com/231664.
    this.webviewNode_.setAttribute('tabIndex', 0);
  }
  var self = this;
  this.webviewNode_.addEventListener('focus', function(e) {
    // Focus the BrowserPlugin when the <webview> takes focus.
    self.browserPluginNode_.focus();
  });
  this.webviewNode_.addEventListener('blur', function(e) {
    // Blur the BrowserPlugin when the <webview> loses focus.
    self.browserPluginNode_.blur();
  });
};

/**
 * @private
 */
WebViewInternal.prototype.canGoBack_ = function() {
  return this.entryCount_ > 1 && this.currentEntryIndex_ > 0;
};

/**
 * @private
 */
WebViewInternal.prototype.canGoForward_ = function() {
  return this.currentEntryIndex_ >= 0 &&
      this.currentEntryIndex_ < (this.entryCount_ - 1);
};

/**
 * @private
 */
WebViewInternal.prototype.getProcessId_ = function() {
  return this.processId_;
};

/**
 * @private
 */
WebViewInternal.prototype.go_ = function(relativeIndex) {
  if (!this.instanceId_) {
    return;
  }
  chrome.webview.go(this.instanceId_, relativeIndex);
};

/**
 * @private
 */
WebViewInternal.prototype.reload_ = function() {
  if (!this.instanceId_) {
    return;
  }
  chrome.webview.reload(this.instanceId_);
};

/**
 * @private
 */
WebViewInternal.prototype.stop_ = function() {
  if (!this.instanceId_) {
    return;
  }
  chrome.webview.stop(this.instanceId_);
};

/**
 * @private
 */
WebViewInternal.prototype.terminate_ = function() {
  if (!this.instanceId_) {
    return;
  }
  chrome.webview.terminate(this.instanceId_);
};

/**
 * @private
 */
WebViewInternal.prototype.validateExecuteCodeCall_  = function() {
  var ERROR_MSG_CANNOT_INJECT_SCRIPT = '<webview>: ' +
      'Script cannot be injected into content until the page has loaded.';
  if (!this.instanceId_) {
    throw new Error(ERROR_MSG_CANNOT_INJECT_SCRIPT);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.executeScript_ = function(var_args) {
  this.validateExecuteCodeCall_();
  var args = $Array.concat([this.instanceId_], $Array.slice(arguments));
  $Function.apply(chrome.webview.executeScript, null, args);
};

/**
 * @private
 */
WebViewInternal.prototype.insertCSS_ = function(var_args) {
  this.validateExecuteCodeCall_();
  var args = $Array.concat([this.instanceId_], $Array.slice(arguments));
  $Function.apply(chrome.webview.insertCSS, null, args);
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebviewNodeProperties_ = function() {
  var ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE = '<webview>: ' +
    'contentWindow is not available at this time. It will become available ' +
        'when the page has finished loading.';

  var browserPluginNode = this.browserPluginNode_;
  // Expose getters and setters for the attributes.
  $Array.forEach(WEB_VIEW_ATTRIBUTES, function(attributeName) {
    Object.defineProperty(this.webviewNode_, attributeName, {
      get: function() {
        if (browserPluginNode.hasOwnProperty(attributeName)) {
          return browserPluginNode[attributeName];
        } else {
          return browserPluginNode.getAttribute(attributeName);
        }
      },
      set: function(value) {
        if (browserPluginNode.hasOwnProperty(attributeName)) {
          // Give the BrowserPlugin first stab at the attribute so that it can
          // throw an exception if there is a problem. This attribute will then
          // be propagated back to the <webview>.
          browserPluginNode[attributeName] = value;
        } else {
          browserPluginNode.setAttribute(attributeName, value);
        }
      },
      enumerable: true
    });
  }, this);

  // We cannot use {writable: true} property descriptor because we want a
  // dynamic getter value.
  Object.defineProperty(this.webviewNode_, 'contentWindow', {
    get: function() {
      if (browserPluginNode.contentWindow)
        return browserPluginNode.contentWindow;
      console.error(ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE);
    },
    // No setter.
    enumerable: true
  });
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebviewNodeAttributes_ = function() {
  Object.defineProperty(this.webviewNode_, 'internal_', {
    enumerable: false,
    writable: false,
    value: function(key) {
      if (key !== secret) {
        return null;
      }
      return this;
    }.bind(this)
  });
  this.setupWebViewSrcAttributeMutationObserver_();
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebViewSrcAttributeMutationObserver_ =
    function() {
  // The purpose of this mutation observer is to catch assignment to the src
  // attribute without any changes to its value. This is useful in the case
  // where the webview guest has crashed and navigating to the same address
  // spawns off a new process.
  var self = this;
  var observer = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, function(mutation) {
      var newValue = self.webviewNode_.getAttribute(mutation.attributeName);
      if (mutation.oldValue != newValue) {
        return;
      }
      self.handleWebviewAttributeMutation_(mutation.attributeName, newValue);
    });
  });
  var params = {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: ['src']
  };
  observer.observe(this.webviewNode_, params);
};

/**
 * @private
 */
WebViewInternal.prototype.handleWebviewAttributeMutation_ =
      function(name, newValue) {
  // This observer monitors mutations to attributes of the <webview> and
  // updates the BrowserPlugin properties accordingly. In turn, updating
  // a BrowserPlugin property will update the corresponding BrowserPlugin
  // attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
  // details.
  if (this.browserPluginNode_.hasOwnProperty(name)) {
    this.browserPluginNode_[name] = newValue;
  } else {
    this.browserPluginNode_.setAttribute(name, newValue);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.handleBrowserPluginAttributeMutation_ =
    function(name, oldValue, newValue) {
  // This observer monitors mutations to attributes of the BrowserPlugin and
  // updates the <webview> attributes accordingly.
  // |newValue| is null if the attribute |name| has been removed.
  if (newValue != null) {
    // Update the <webview> attribute to match the BrowserPlugin attribute.
    // Note: Calling setAttribute on <webview> will trigger its mutation
    // observer which will then propagate that attribute to BrowserPlugin. In
    // cases where we permit assigning a BrowserPlugin attribute the same value
    // again (such as navigation when crashed), this could end up in an infinite
    // loop. Thus, we avoid this loop by only updating the <webview> attribute
    // if the BrowserPlugin attributes differs from it.
    if (newValue != oldValue) {
      this.webviewNode_.setAttribute(name, newValue);
    }
  } else {
    // If an attribute is removed from the BrowserPlugin, then remove it
    // from the <webview> as well.
    this.webviewNode_.removeAttribute(name);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.getWebviewExtEvents_ = function() {
  var experimentalExtEvents = this.maybeGetWebviewExperimentalExtEvents_();
  for (var eventName in experimentalExtEvents) {
    WEB_VIEW_EXT_EVENTS[eventName] = experimentalExtEvents[eventName];
  }
  return WEB_VIEW_EXT_EVENTS;
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebviewNodeEvents_ = function() {
  var self = this;
  this.viewInstanceId_ = ++webViewInstanceIdCounter;
  var onInstanceIdAllocated = function(e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    self.instanceId_ = detail.windowId;
    var params = {
      'api': 'webview',
      'instanceId': self.viewInstanceId_
    };
    self.browserPluginNode_['-internal-attach'](params);

    var extEvents = self.getWebviewExtEvents_();
    for (var eventName in extEvents) {
      self.setupExtEvent_(eventName, extEvents[eventName]);
    }
  };
  this.browserPluginNode_.addEventListener('-internal-instanceid-allocated',
                                           onInstanceIdAllocated);
  this.setupWebRequestEvents_();
};

/**
 * @private
 */
WebViewInternal.prototype.setupExtEvent_ = function(eventName, eventInfo) {
  var self = this;
  var webviewNode = this.webviewNode_;
  eventInfo.evt.addListener(function(event) {
    var details = {bubbles:true};
    if (eventInfo.cancelable)
      details.cancelable = true;
    var webviewEvent = new Event(eventName, details);
    $Array.forEach(eventInfo.fields, function(field) {
      if (event[field] !== undefined) {
        webviewEvent[field] = event[field];
      }
    });
    if (eventInfo.customHandler) {
      eventInfo.customHandler(self, event, webviewEvent);
      return;
    }
    webviewNode.dispatchEvent(webviewEvent);
  }, {instanceId: self.instanceId_});
};

/**
 * @private
 */
WebViewInternal.prototype.setupExtNewWindowEvent_ =
    function(event, webviewEvent) {
  var ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN = '<webview>: ' +
      'An action has already been taken for this "newwindow" event.';

  var ERROR_MSG_NEWWINDOW_UNABLE_TO_ATTACH = '<webview>: ' +
      'Unable to attach the new window to the provided webview.';

  var ERROR_MSG_WEBVIEW_EXPECTED = '<webview> element expected.';

  var showWarningMessage = function() {
    var WARNING_MSG_NEWWINDOW_BLOCKED = '<webview>: A new window was blocked.';
    console.warn(WARNING_MSG_NEWWINDOW_BLOCKED);
  };

  var self = this;
  var browserPluginNode = this.browserPluginNode_;
  var webviewNode = this.webviewNode_;

  var requestId = event.requestId;
  var actionTaken = false;

  var validateCall = function () {
    if (actionTaken) {
      throw new Error(ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN);
    }
    actionTaken = true;
  };

  var window = {
    attach: function(webview) {
      validateCall();
      if (!webview)
        throw new Error(ERROR_MSG_WEBVIEW_EXPECTED);
      // Attach happens asynchronously to give the tagWatcher an opportunity
      // to pick up the new webview before attach operates on it, if it hasn't
      // been attached to the DOM already.
      // Note: Any subsequent errors cannot be exceptions because they happen
      // asynchronously.
      setTimeout(function() {
        var attached =
            browserPluginNode['-internal-attachWindowTo'](webview,
                                                          event.windowId);
        if (!attached) {
          console.error(ERROR_MSG_NEWWINDOW_UNABLE_TO_ATTACH);
        }
        // If the object being passed into attach is not a valid <webview>
        // then we will fail and it will be treated as if the new window
        // was rejected. The permission API plumbing is used here to clean
        // up the state created for the new window if attaching fails.
        chrome.webview.setPermission(self.instanceId_, requestId, attached, '');
      }, 0);
    },
    discard: function() {
      validateCall();
      chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    }
  };
  webviewEvent.window = window;

  var defaultPrevented = !webviewNode.dispatchEvent(webviewEvent);
  if (actionTaken) {
    return;
  }

  if (defaultPrevented) {
    // Make browser plugin track lifetime of |window|.
    messagingNatives.BindToGC(window, function() {
      // Avoid showing a warning message if the decision has already been made.
      if (actionTaken) {
        return;
      }
      chrome.webview.setPermission(self.instanceId_, requestId, false, '');
      showWarningMessage();
    });
  } else {
    actionTaken = true;
    // The default action is to discard the window.
    chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    showWarningMessage();
  }
};

WebViewInternal.prototype.getPermissionTypes_ = function() {
  return ['media', 'geolocation', 'pointerLock', 'download'];
};

WebViewInternal.prototype.setupExtPermissionEvent_ =
    function(event, webviewEvent) {
  var ERROR_MSG_PERMISSION_ALREADY_DECIDED = '<webview>: ' +
      'Permission has already been decided for this "permissionrequest" event.';

  var showWarningMessage = function(permission) {
    var WARNING_MSG_PERMISSION_DENIED = '<webview>: ' +
        'The permission request for "%1" has been denied.';
    console.warn(WARNING_MSG_PERMISSION_DENIED.replace('%1', permission));
  };

  var PERMISSION_TYPES = this.getPermissionTypes_();

  var self = this;
  var browserPluginNode = this.browserPluginNode_;
  var webviewNode = this.webviewNode_;

  var requestId = event.requestId;
  var decisionMade = false;

  var validateCall = function() {
    if (decisionMade) {
      throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
    }
    decisionMade = true;
  };

  // Construct the event.request object.
  var request = {
    allow: function() {
      validateCall();
      chrome.webview.setPermission(self.instanceId_, requestId, true, '');
    },
    deny: function() {
      validateCall();
      chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    }
  };
  webviewEvent.request = request;

  var defaultPrevented = !webviewNode.dispatchEvent(webviewEvent);
  if (decisionMade) {
    return;
  }

  if (defaultPrevented) {
    // Make browser plugin track lifetime of |request|.
    messagingNatives.BindToGC(request, function() {
      // Avoid showing a warning message if the decision has already been made.
      if (decisionMade) {
        return;
      }
      chrome.webview.setPermission(self.instanceId_, requestId, false, '');
      showWarningMessage(event.permission);
    });
  } else {
    decisionMade = true;
    chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    showWarningMessage(event.permission);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebRequestEvents_ = function() {
  var self = this;
  var request = {};
  var createWebRequestEvent = function(webRequestEvent) {
    return function() {
      if (!self[webRequestEvent.name + '_']) {
        self[webRequestEvent.name + '_'] =
            new WebRequestEvent(
                'webview.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.extraParameters, null,
                self.viewInstanceId_);
      }
      return self[webRequestEvent.name + '_'];
    };
  };

  // Populate the WebRequest events from the API definition.
  for (var i = 0; i < webRequestSchema.events.length; ++i) {
    var webRequestEvent = createWebRequestEvent(webRequestSchema.events[i]);
    Object.defineProperty(
        request,
        webRequestSchema.events[i].name,
        {
          get: webRequestEvent,
          enumerable: true
        }
    );
    this.maybeAttachWebRequestEventToWebview_(webRequestSchema.events[i].name,
                                              webRequestEvent);
  }
  Object.defineProperty(
      this.webviewNode_,
      'request',
      {
        value: request,
        enumerable: true,
        writable: false
      }
  );
};

// Save document.register in a variable in case the developer attempts to
// override it at some point.
var register = document.register;

// Registers browser plugin <object> custom element.
function registerBrowserPluginElement() {
  var proto = Object.create(HTMLObjectElement.prototype);

  proto.createdCallback = function() {
    this.setAttribute('type', 'application/browser-plugin');
    // The <object> node fills in the <webview> container.
    this.style.width = '100%';
    this.style.height = '100%';
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    if (!this.internal_) {
      return;
    }
    var internal = this.internal_(secret);
    internal.handleBrowserPluginAttributeMutation_(name, oldValue, newValue);
  };

  WebViewInternal.BrowserPlugin =
      register.call(document, 'browser-plugin', {prototype: proto});

  delete proto.createdCallback;
  delete proto.enteredDocumentCallback;
  delete proto.leftDocumentCallback;
  delete proto.attributeChangedCallback;
}

// Registers <webview> custom element.
function registerWebViewElement() {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    new WebViewInternal(this);
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = this.internal_(secret);
    internal.handleWebviewAttributeMutation_(name, newValue);
  };

  proto.back = function() {
    this.go(-1);
  };

  proto.forward = function() {
    this.go(1);
  };

  proto.canGoBack = function() {
    return this.internal_(secret).canGoBack_();
  };

  proto.canGoForward = function() {
    return this.internal_(secret).canGoForward_();
  };

  proto.getProcessId = function() {
    return this.internal_(secret).getProcessId_();
  };

  proto.go = function(relativeIndex) {
    this.internal_(secret).go_(relativeIndex);
  };

  proto.reload = function() {
    this.internal_(secret).reload_();
  };

  proto.stop = function() {
    this.internal_(secret).stop_();
  };

  proto.terminate = function() {
    this.internal_(secret).terminate_();
  };

  proto.executeScript = function(var_args) {
    var internal = this.internal_(secret);
    $Function.apply(internal.executeScript_, internal, arguments);
  };

  proto.insertCSS = function(var_args) {
    var internal = this.internal_(secret);
    $Function.apply(internal.insertCSS_, internal, arguments);
  };
  WebViewInternal.maybeRegisterExperimentalAPIs(proto, secret);

  window.WebView = register.call(document, 'webview', {prototype: proto});

  // Delete the callbacks so developers cannot call them and produce unexpected
  // behavior.
  delete proto.createdCallback;
  delete proto.enteredDocumentCallback;
  delete proto.leftDocumentCallback;
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

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebViewInternal.prototype.maybeSetupExtDialogEvent_ = function() {};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebViewInternal.prototype.maybeGetWebviewExperimentalExtEvents_ = function() {};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebViewInternal.prototype.maybeAttachWebRequestEventToWebview_ = function() {};

exports.WebViewInternal = WebViewInternal;
exports.CreateEvent = createEvent;
