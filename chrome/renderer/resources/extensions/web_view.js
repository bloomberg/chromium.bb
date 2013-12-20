// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

'use strict';

var DocumentNatives = requireNative('document_natives');
var EventBindings = require('event_bindings');
var IdGenerator = requireNative('id_generator');
var MessagingNatives = requireNative('messaging_natives');
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var WebRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var DeclarativeWebRequestSchema =
    requireNative('schema_registry').GetSchema('declarativeWebRequest');
var WebView = require('binding').Binding.create('webview').generate();

// This secret enables hiding <webview> private members from the outside scope.
// Outside of this file, |secret| is inaccessible. The only way to access the
// <webview> element's internal members is via the |secret|. Since it's only
// accessible by code here (and in web_view_experimental), only <webview>'s
// API can access it and not external developers.
var secret = {};

var WEB_VIEW_ATTRIBUTE_MAXHEIGHT = 'maxheight';
var WEB_VIEW_ATTRIBUTE_MAXWIDTH = 'maxwidth';
var WEB_VIEW_ATTRIBUTE_MINHEIGHT = 'minheight';
var WEB_VIEW_ATTRIBUTE_MINWIDTH = 'minwidth';

/** @type {Array.<string>} */
var WEB_VIEW_ATTRIBUTES = [
    'allowtransparency',
    'autosize',
    'name',
    'partition',
    WEB_VIEW_ATTRIBUTE_MINHEIGHT,
    WEB_VIEW_ATTRIBUTE_MINWIDTH,
    WEB_VIEW_ATTRIBUTE_MAXHEIGHT,
    WEB_VIEW_ATTRIBUTE_MAXWIDTH
];

var CreateEvent = function(name) {
  var eventOpts = {supportsListeners: true, supportsFilters: true};
  return new EventBindings.Event(name, undefined, eventOpts);
};

var WEB_VIEW_EVENTS = {
  'close': {
    evt: CreateEvent('webview.onClose'),
    fields: []
  },
  'consolemessage': {
    evt: CreateEvent('webview.onConsoleMessage'),
    fields: ['level', 'message', 'line', 'sourceId']
  },
  'contentload': {
    evt: CreateEvent('webview.onContentLoad'),
    fields: []
  },
  'exit': {
     evt: CreateEvent('webview.onExit'),
     fields: ['processId', 'reason']
  },
  'loadabort': {
    cancelable: true,
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handleLoadAbortEvent_(event, webViewEvent);
    },
    evt: CreateEvent('webview.onLoadAbort'),
    fields: ['url', 'isTopLevel', 'reason']
  },
  'loadcommit': {
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handleLoadCommitEvent_(event, webViewEvent);
    },
    evt: CreateEvent('webview.onLoadCommit'),
    fields: ['url', 'isTopLevel']
  },
  'loadprogress': {
    evt: CreateEvent('webview.onLoadProgress'),
    fields: ['url', 'progress']
  },
  'loadredirect': {
    evt: CreateEvent('webview.onLoadRedirect'),
    fields: ['isTopLevel', 'oldUrl', 'newUrl']
  },
  'loadstart': {
    evt: CreateEvent('webview.onLoadStart'),
    fields: ['url', 'isTopLevel']
  },
  'loadstop': {
    evt: CreateEvent('webview.onLoadStop'),
    fields: []
  },
  'newwindow': {
    cancelable: true,
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handleNewWindowEvent_(event, webViewEvent);
    },
    evt: CreateEvent('webview.onNewWindow'),
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
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handlePermissionEvent_(event, webViewEvent);
    },
    evt: CreateEvent('webview.onPermissionRequest'),
    fields: [
      'identifier',
      'lastUnlockedBySelf',
      'name',
      'permission',
      'requestMethod',
      'url',
      'userGesture'
    ]
  },
  'responsive': {
    evt: CreateEvent('webview.onResponsive'),
    fields: ['processId']
  },
  'sizechanged': {
    evt: CreateEvent('webview.onSizeChanged'),
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handleSizeChangedEvent_(event, webViewEvent);
    },
    fields: ['oldHeight', 'oldWidth', 'newHeight', 'newWidth']
  },
  'unresponsive': {
    evt: CreateEvent('webview.onUnresponsive'),
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

  var ALL_ATTRIBUTES = WEB_VIEW_ATTRIBUTES.concat(['src']);
  $Array.forEach(ALL_ATTRIBUTES, function(attributeName) {
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
    // <webview> needs a tabIndex in order to be focusable.
    // TODO(fsamuel): It would be nice to avoid exposing a tabIndex attribute
    // to allow <webview> to be focusable.
    // See http://crbug.com/231664.
    this.webviewNode_.setAttribute('tabIndex', -1);
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
  WebView.go(this.instanceId_, relativeIndex);
};

/**
 * @private
 */
WebViewInternal.prototype.reload_ = function() {
  if (!this.instanceId_) {
    return;
  }
  WebView.reload(this.instanceId_);
};

/**
 * @private
 */
WebViewInternal.prototype.stop_ = function() {
  if (!this.instanceId_) {
    return;
  }
  WebView.stop(this.instanceId_);
};

/**
 * @private
 */
WebViewInternal.prototype.terminate_ = function() {
  if (!this.instanceId_) {
    return;
  }
  WebView.terminate(this.instanceId_);
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
  $Function.apply(WebView.executeScript, null, args);
};

/**
 * @private
 */
WebViewInternal.prototype.insertCSS_ = function(var_args) {
  this.validateExecuteCodeCall_();
  var args = $Array.concat([this.instanceId_], $Array.slice(arguments));
  $Function.apply(WebView.insertCSS, null, args);
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebviewNodeProperties_ = function() {
  var ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE = '<webview>: ' +
    'contentWindow is not available at this time. It will become available ' +
        'when the page has finished loading.';

  var self = this;
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

  // <webview> src does not quite behave the same as BrowserPlugin src, and so
  // we don't simply keep the two in sync.
  this.src_ = this.webviewNode_.getAttribute('src');
  Object.defineProperty(this.webviewNode_, 'src', {
    get: function() {
      return self.src_;
    },
    set: function(value) {
      self.webviewNode_.setAttribute('src', value);
    },
    // No setter.
    enumerable: true
  });

  // We cannot use {writable: true} property descriptor because we want a
  // dynamic getter value.
  Object.defineProperty(this.webviewNode_, 'contentWindow', {
    get: function() {
      if (browserPluginNode.contentWindow)
        return browserPluginNode.contentWindow;
      window.console.error(ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE);
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
  this.srcObserver_ = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, function(mutation) {
      var oldValue = mutation.oldValue;
      var newValue = self.webviewNode_.getAttribute(mutation.attributeName);
      if (oldValue != newValue) {
        return;
      }
      self.handleWebviewAttributeMutation_(
          mutation.attributeName, oldValue, newValue);
    });
  });
  var params = {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: ['src']
  };
  this.srcObserver_.observe(this.webviewNode_, params);
};

/**
 * @private
 */
WebViewInternal.prototype.handleWebviewAttributeMutation_ =
      function(name, oldValue, newValue) {
  // This observer monitors mutations to attributes of the <webview> and
  // updates the BrowserPlugin properties accordingly. In turn, updating
  // a BrowserPlugin property will update the corresponding BrowserPlugin
  // attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
  // details.
  if (name == 'src') {
    // We treat null attribute (attribute removed) and the empty string as
    // one case.
    oldValue = oldValue || '';
    newValue = newValue || '';
    // Once we have navigated, we don't allow clearing the src attribute.
    // Once <webview> enters a navigated state, it cannot be return back to a
    // placeholder state.
    if (newValue == '' && oldValue != '') {
      // src attribute changes normally initiate a navigation. We suppress
      // the next src attribute handler call to avoid reloading the page
      // on every guest-initiated navigation.
      this.ignoreNextSrcAttributeChange_ = true;
      this.webviewNode_.setAttribute('src', oldValue);
      return;
    }
    this.src_ = newValue;
    if (this.ignoreNextSrcAttributeChange_) {
      // Don't allow the src mutation observer to see this change.
      this.srcObserver_.takeRecords();
      this.ignoreNextSrcAttributeChange_ = false;
      return;
    }
  }
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
    function(name, newValue) {
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
    if (newValue != this.webviewNode_.getAttribute(name)) {
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
WebViewInternal.prototype.getEvents_ = function() {
  var experimentalEvents = this.maybeGetExperimentalEvents_();
  for (var eventName in experimentalEvents) {
    WEB_VIEW_EVENTS[eventName] = experimentalEvents[eventName];
  }
  return WEB_VIEW_EVENTS;
};

WebViewInternal.prototype.handleSizeChangedEvent_ =
    function(event, webViewEvent) {
  var node = this.webviewNode_;

  var width = node.offsetWidth;
  var height = node.offsetHeight;

  // Check the current bounds to make sure we do not resize <webview>
  // outside of current constraints.
  var maxWidth;
  if (node.hasAttribute(WEB_VIEW_ATTRIBUTE_MAXWIDTH) &&
      node[WEB_VIEW_ATTRIBUTE_MAXWIDTH]) {
    maxWidth = node[WEB_VIEW_ATTRIBUTE_MAXWIDTH];
  } else {
    maxWidth = width;
  }

  var minWidth;
  if (node.hasAttribute(WEB_VIEW_ATTRIBUTE_MINWIDTH) &&
      node[WEB_VIEW_ATTRIBUTE_MINWIDTH]) {
    minWidth = node[WEB_VIEW_ATTRIBUTE_MINWIDTH];
  } else {
    minWidth = width;
  }
  if (minWidth > maxWidth) {
    minWidth = maxWidth;
  }

  var maxHeight;
  if (node.hasAttribute(WEB_VIEW_ATTRIBUTE_MAXHEIGHT) &&
      node[WEB_VIEW_ATTRIBUTE_MAXHEIGHT]) {
    maxHeight = node[WEB_VIEW_ATTRIBUTE_MAXHEIGHT];
  } else {
    maxHeight = height;
  }
  var minHeight;
  if (node.hasAttribute(WEB_VIEW_ATTRIBUTE_MINHEIGHT) &&
      node[WEB_VIEW_ATTRIBUTE_MINHEIGHT]) {
    minHeight = node[WEB_VIEW_ATTRIBUTE_MINHEIGHT];
  } else {
    minHeight = height;
  }
  if (minHeight > maxHeight) {
    minHeight = maxHeight;
  }

  if (webViewEvent.newWidth >= minWidth &&
      webViewEvent.newWidth <= maxWidth &&
      webViewEvent.newHeight >= minHeight &&
      webViewEvent.newHeight <= maxHeight) {
    node.style.width = webViewEvent.newWidth + 'px';
    node.style.height = webViewEvent.newHeight + 'px';
  }
  node.dispatchEvent(webViewEvent);
};

/**
 * @private
 */
WebViewInternal.prototype.setupWebviewNodeEvents_ = function() {
  var self = this;
  this.viewInstanceId_ = IdGenerator.GetNextId();
  var onInstanceIdAllocated = function(e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    self.instanceId_ = detail.windowId;
    var params = {
      'api': 'webview',
      'instanceId': self.viewInstanceId_
    };
    if (self.userAgentOverride_) {
      params['userAgentOverride'] = self.userAgentOverride_;
    }
    self.browserPluginNode_['-internal-attach'](params);

    var events = self.getEvents_();
    for (var eventName in events) {
      self.setupEvent_(eventName, events[eventName]);
    }
  };
  this.browserPluginNode_.addEventListener('-internal-instanceid-allocated',
                                           onInstanceIdAllocated);
  this.setupWebRequestEvents_();

  this.on_ = {};
  var events = self.getEvents_();
  for (var eventName in events) {
    this.setupEventProperty_(eventName);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.setupEvent_ = function(eventName, eventInfo) {
  var self = this;
  var webviewNode = this.webviewNode_;
  eventInfo.evt.addListener(function(event) {
    var details = {bubbles:true};
    if (eventInfo.cancelable)
      details.cancelable = true;
    var webViewEvent = new Event(eventName, details);
    $Array.forEach(eventInfo.fields, function(field) {
      if (event[field] !== undefined) {
        webViewEvent[field] = event[field];
      }
    });
    if (eventInfo.customHandler) {
      eventInfo.customHandler(self, event, webViewEvent);
      return;
    }
    webviewNode.dispatchEvent(webViewEvent);
  }, {instanceId: self.instanceId_});
};

/**
 * Adds an 'on<event>' property on the webview, which can be used to set/unset
 * an event handler.
 * @private
 */
WebViewInternal.prototype.setupEventProperty_ = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  var self = this;
  var webviewNode = this.webviewNode_;
  Object.defineProperty(webviewNode, propertyName, {
    get: function() {
      return self.on_[propertyName];
    },
    set: function(value) {
      if (self.on_[propertyName])
        webviewNode.removeEventListener(eventName, self.on_[propertyName]);
      self.on_[propertyName] = value;
      if (value)
        webviewNode.addEventListener(eventName, value);
    },
    enumerable: true
  });
};

/**
 * @private
 */
WebViewInternal.prototype.getPermissionTypes_ = function() {
  return ['media', 'geolocation', 'pointerLock', 'download', 'loadplugin'];
};

/**
 * @private
 */
WebViewInternal.prototype.handleLoadAbortEvent_ =
    function(event, webViewEvent) {
  var showWarningMessage = function(reason) {
    var WARNING_MSG_LOAD_ABORTED = '<webview>: ' +
        'The load has aborted with reason "%1".';
    window.console.warn(WARNING_MSG_LOAD_ABORTED.replace('%1', reason));
  };
  if (this.webviewNode_.dispatchEvent(webViewEvent)) {
    showWarningMessage(event.reason);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.handleLoadCommitEvent_ =
    function(event, webViewEvent) {
  this.currentEntryIndex_ = event.currentEntryIndex;
  this.entryCount_ = event.entryCount;
  this.processId_ = event.processId;
  var oldValue = this.webviewNode_.getAttribute('src');
  var newValue = event.url;
  if (event.isTopLevel && (oldValue != newValue)) {
    // Touching the src attribute triggers a navigation. To avoid
    // triggering a page reload on every guest-initiated navigation,
    // we use the flag ignoreNextSrcAttributeChange_ here.
    this.ignoreNextSrcAttributeChange_ = true;
    this.webviewNode_.setAttribute('src', newValue);
  }
  this.webviewNode_.dispatchEvent(webViewEvent);
}

/**
 * @private
 */
WebViewInternal.prototype.handleNewWindowEvent_ =
    function(event, webViewEvent) {
  var ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN = '<webview>: ' +
      'An action has already been taken for this "newwindow" event.';

  var ERROR_MSG_NEWWINDOW_UNABLE_TO_ATTACH = '<webview>: ' +
      'Unable to attach the new window to the provided webview.';

  var ERROR_MSG_WEBVIEW_EXPECTED = '<webview> element expected.';

  var showWarningMessage = function() {
    var WARNING_MSG_NEWWINDOW_BLOCKED = '<webview>: A new window was blocked.';
    window.console.warn(WARNING_MSG_NEWWINDOW_BLOCKED);
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

  var windowObj = {
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
          window.console.error(ERROR_MSG_NEWWINDOW_UNABLE_TO_ATTACH);
        }
        // If the object being passed into attach is not a valid <webview>
        // then we will fail and it will be treated as if the new window
        // was rejected. The permission API plumbing is used here to clean
        // up the state created for the new window if attaching fails.
        WebView.setPermission(
            self.instanceId_, requestId, attached ? 'allow' : 'deny');
      }, 0);
    },
    discard: function() {
      validateCall();
      WebView.setPermission(self.instanceId_, requestId, 'deny');
    }
  };
  webViewEvent.window = windowObj;

  var defaultPrevented = !webviewNode.dispatchEvent(webViewEvent);
  if (actionTaken) {
    return;
  }

  if (defaultPrevented) {
    // Make browser plugin track lifetime of |windowObj|.
    MessagingNatives.BindToGC(windowObj, function() {
      // Avoid showing a warning message if the decision has already been made.
      if (actionTaken) {
        return;
      }
      WebView.setPermission(
          self.instanceId_, requestId, 'default', '', function(allowed) {
        if (allowed) {
          return;
        }
        showWarningMessage();
      });
    });
  } else {
    actionTaken = true;
    // The default action is to discard the window.
    WebView.setPermission(
        self.instanceId_, requestId, 'default', '', function(allowed) {
      if (allowed) {
        return;
      }
      showWarningMessage();
    });
  }
};

WebViewInternal.prototype.handlePermissionEvent_ =
    function(event, webViewEvent) {
  var ERROR_MSG_PERMISSION_ALREADY_DECIDED = '<webview>: ' +
      'Permission has already been decided for this "permissionrequest" event.';

  var showWarningMessage = function(permission) {
    var WARNING_MSG_PERMISSION_DENIED = '<webview>: ' +
        'The permission request for "%1" has been denied.';
    window.console.warn(
        WARNING_MSG_PERMISSION_DENIED.replace('%1', permission));
  };

  var requestId = event.requestId;
  var self = this;

  var PERMISSION_TYPES = this.getPermissionTypes_().concat(
                             this.maybeGetExperimentalPermissions_());
  if (PERMISSION_TYPES.indexOf(event.permission) < 0) {
    // The permission type is not allowed. Trigger the default response.
    WebView.setPermission(
        self.instanceId_, requestId, 'default', '', function(allowed) {
      if (allowed) {
        return;
      }
      showWarningMessage(event.permission);
    });
    return;
  }

  var browserPluginNode = this.browserPluginNode_;
  var webviewNode = this.webviewNode_;

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
      WebView.setPermission(self.instanceId_, requestId, 'allow');
    },
    deny: function() {
      validateCall();
      WebView.setPermission(self.instanceId_, requestId, 'deny');
    }
  };
  webViewEvent.request = request;

  var defaultPrevented = !webviewNode.dispatchEvent(webViewEvent);
  if (decisionMade) {
    return;
  }

  if (defaultPrevented) {
    // Make browser plugin track lifetime of |request|.
    MessagingNatives.BindToGC(request, function() {
      // Avoid showing a warning message if the decision has already been made.
      if (decisionMade) {
        return;
      }
      WebView.setPermission(
          self.instanceId_, requestId, 'default', '', function(allowed) {
        if (allowed) {
          return;
        }
        showWarningMessage(event.permission);
      });
    });
  } else {
    decisionMade = true;
    WebView.setPermission(
        self.instanceId_, requestId, 'default', '', function(allowed) {
      if (allowed) {
        return;
      }
      showWarningMessage(event.permission);
    });
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
                webRequestEvent.extraParameters, webRequestEvent.options,
                self.viewInstanceId_);
      }
      return self[webRequestEvent.name + '_'];
    };
  };

  for (var i = 0; i < DeclarativeWebRequestSchema.events.length; ++i) {
    var eventSchema = DeclarativeWebRequestSchema.events[i];
    var webRequestEvent = createWebRequestEvent(eventSchema);
    this.maybeAttachWebRequestEventToObject_(request,
                                             eventSchema.name,
                                             webRequestEvent);
  }

  // Populate the WebRequest events from the API definition.
  for (var i = 0; i < WebRequestSchema.events.length; ++i) {
    var webRequestEvent = createWebRequestEvent(WebRequestSchema.events[i]);
    Object.defineProperty(
        request,
        WebRequestSchema.events[i].name,
        {
          get: webRequestEvent,
          enumerable: true
        }
    );
    this.maybeAttachWebRequestEventToObject_(this.webviewNode_,
                                             WebRequestSchema.events[i].name,
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
    internal.handleBrowserPluginAttributeMutation_(name, newValue);
  };

  proto.attachedCallback = function() {
    // Load the plugin immediately.
    var unused = this.nonExistentAttribute;
  };

  // TODO(dominicc): Remove this line once Custom Elements renames
  // enteredViewCallback to attachedCallback
  proto.enteredViewCallback = proto.attachedCallback;

  WebViewInternal.BrowserPlugin =
      DocumentNatives.RegisterElement('browser-plugin', {extends: 'object',
                                                         prototype: proto});

  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;

  // TODO(dominicc): Remove these lines once Custom Elements renames
  // enteredView, leftView callbacks to attached, detached
  // respectively.
  delete proto.enteredViewCallback;
  delete proto.leftViewCallback;
}

// Registers <webview> custom element.
function registerWebViewElement() {
  var proto = Object.create(HTMLElement.prototype);

  proto.createdCallback = function() {
    new WebViewInternal(this);
  };

  proto.attributeChangedCallback = function(name, oldValue, newValue) {
    var internal = this.internal_(secret);
    internal.handleWebviewAttributeMutation_(name, oldValue, newValue);
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

  window.WebView =
      DocumentNatives.RegisterElement('webview', {prototype: proto});

  // Delete the callbacks so developers cannot call them and produce unexpected
  // behavior.
  delete proto.createdCallback;
  delete proto.attachedCallback;
  delete proto.detachedCallback;
  delete proto.attributeChangedCallback;

  // TODO(dominicc): Remove these lines once Custom Elements renames
  // enteredView, leftView callbacks to attached, detached
  // respectively.
  delete proto.enteredViewCallback;
  delete proto.leftViewCallback;
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
WebViewInternal.prototype.maybeGetExperimentalEvents_ = function() {};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebViewInternal.prototype.maybeAttachWebRequestEventToObject_ = function() {};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebViewInternal.prototype.maybeGetExperimentalPermissions_ = function() {
  return [];
};

exports.WebView = WebView;
exports.WebViewInternal = WebViewInternal;
exports.CreateEvent = CreateEvent;
