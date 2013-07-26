// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim that simulates a <webview> tag via Mutation Observers.
//
// The actual tag is implemented via the browser plugin. The internals of this
// are hidden via Shadow DOM.

var addTagWatcher = require('tagWatcher').addTagWatcher;
var eventBindings = require('event_bindings');

/** @type {Array.<string>} */
var WEB_VIEW_ATTRIBUTES = ['name', 'src', 'partition', 'autosize', 'minheight',
    'minwidth', 'maxheight', 'maxwidth'];

var WEB_VIEW_EVENTS = {
  'sizechanged': ['oldHeight', 'oldWidth', 'newHeight', 'newWidth'],
};

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
    customHandler: function(webview, event) {
      webview.currentEntryIndex_ = event.currentEntryIndex;
      webview.entryCount_ = event.entryCount;
      webview.processId_ = event.processId;
      if (event.isTopLevel) {
        webview.browserPluginNode_.setAttribute('src', event.url);
      }
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
  'responsive': {
    evt: createEvent('webview.onResponsive'),
    fields: ['processId']
  },
  'unresponsive': {
    evt: createEvent('webview.onUnresponsive'),
    fields: ['processId']
  }
};

addTagWatcher('WEBVIEW', function(addedNode) { new WebView(addedNode); });

/** @type {number} */
WebView.prototype.entryCount_;

/** @type {number} */
WebView.prototype.currentEntryIndex_;

/** @type {number} */
WebView.prototype.processId_;

/**
 * @constructor
 */
function WebView(webviewNode) {
  this.webviewNode_ = webviewNode;
  this.browserPluginNode_ = this.createBrowserPluginNode_();
  var shadowRoot = this.webviewNode_.webkitCreateShadowRoot();
  shadowRoot.appendChild(this.browserPluginNode_);

  this.setupFocusPropagation_();
  this.setupWebviewNodeMethods_();
  this.setupWebviewNodeProperties_();
  this.setupWebviewNodeAttributes_();
  this.setupWebviewNodeEvents_();

  // Experimental API
  this.maybeSetupExperimentalAPI_();
}

/**
 * @private
 */
WebView.prototype.createBrowserPluginNode_ = function() {
  var browserPluginNode = document.createElement('object');
  browserPluginNode.type = 'application/browser-plugin';
  // The <object> node fills in the <webview> container.
  browserPluginNode.style.width = '100%';
  browserPluginNode.style.height = '100%';
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
WebView.prototype.setupFocusPropagation_ = function() {
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
WebView.prototype.setupWebviewNodeMethods_ = function() {
  // this.browserPluginNode_[apiMethod] are not necessarily defined immediately
  // after the shadow object is appended to the shadow root.
  var webviewNode = this.webviewNode_;
  var browserPluginNode = this.browserPluginNode_;
  var self = this;

  webviewNode['canGoBack'] = function() {
    return self.entryCount_ > 1 && self.currentEntryIndex_ > 0;
  };

  webviewNode['canGoForward'] = function() {
    return self.currentEntryIndex_ >=0 &&
        self.currentEntryIndex_ < (self.entryCount_ - 1);
  };

  webviewNode['back'] = function() {
    webviewNode.go(-1);
  };

  webviewNode['forward'] = function() {
    webviewNode.go(1);
  };

  webviewNode['getProcessId'] = function() {
    return self.processId_;
  };

  webviewNode['go'] = function(relativeIndex) {
    var instanceId = browserPluginNode.getGuestInstanceId();
    if (!instanceId) {
      return;
    }
    chrome.webview.go(instanceId, relativeIndex);
  };

  webviewNode['reload'] = function() {
    var instanceId = browserPluginNode.getGuestInstanceId();
    if (!instanceId) {
      return;
    }
    chrome.webview.reload(instanceId);
  };

  webviewNode['stop'] = function() {
    var instanceId = browserPluginNode.getGuestInstanceId();
    if (!instanceId) {
      return;
    }
    chrome.webview.stop(instanceId);
  };

  webviewNode['terminate'] = function() {
    var instanceId = browserPluginNode.getGuestInstanceId();
    if (!instanceId) {
      return;
    }
    chrome.webview.terminate(instanceId);
  };

  this.setupExecuteCodeAPI_();
};

/**
 * @private
 */
WebView.prototype.setupWebviewNodeProperties_ = function() {
  var ERROR_MSG_CONTENTWINDOW_NOT_AVAILABLE = '<webview>: ' +
    'contentWindow is not available at this time. It will become available ' +
        'when the page has finished loading.';

  var browserPluginNode = this.browserPluginNode_;
  // Expose getters and setters for the attributes.
  $Array.forEach(WEB_VIEW_ATTRIBUTES, function(attributeName) {
    Object.defineProperty(this.webviewNode_, attributeName, {
      get: function() {
        return browserPluginNode[attributeName];
      },
      set: function(value) {
        browserPluginNode[attributeName] = value;
      },
      enumerable: true
    });
  }, this);

  // We cannot use {writable: true} property descriptor because we want dynamic
  // getter value.
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
WebView.prototype.setupWebviewNodeAttributes_ = function() {
  this.setupWebviewNodeObservers_();
  this.setupBrowserPluginNodeObservers_();
};

/**
 * @private
 */
WebView.prototype.setupWebviewNodeObservers_ = function() {
  // Map attribute modifications on the <webview> tag to property changes in
  // the underlying <object> node.
  var handleMutation = $Function.bind(function(mutation) {
    this.handleWebviewAttributeMutation_(mutation);
  }, this);
  var observer = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, handleMutation);
  });
  observer.observe(
      this.webviewNode_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});
};

/**
 * @private
 */
WebView.prototype.setupBrowserPluginNodeObservers_ = function() {
  var handleMutation = $Function.bind(function(mutation) {
    this.handleBrowserPluginAttributeMutation_(mutation);
  }, this);
  var objectObserver = new MutationObserver(function(mutations) {
    $Array.forEach(mutations, handleMutation);
  });
  objectObserver.observe(
      this.browserPluginNode_,
      {attributes: true, attributeFilter: WEB_VIEW_ATTRIBUTES});
};

/**
 * @private
 */
WebView.prototype.handleWebviewAttributeMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the <webview> and
  // updates the BrowserPlugin properties accordingly. In turn, updating
  // a BrowserPlugin property will update the corresponding BrowserPlugin
  // attribute, if necessary. See BrowserPlugin::UpdateDOMAttribute for more
  // details.
  this.browserPluginNode_[mutation.attributeName] =
      this.webviewNode_.getAttribute(mutation.attributeName);
};

/**
 * @private
 */
WebView.prototype.handleBrowserPluginAttributeMutation_ = function(mutation) {
  // This observer monitors mutations to attributes of the BrowserPlugin and
  // updates the <webview> attributes accordingly.
  if (!this.browserPluginNode_.hasAttribute(mutation.attributeName)) {
    // If an attribute is removed from the BrowserPlugin, then remove it
    // from the <webview> as well.
    this.webviewNode_.removeAttribute(mutation.attributeName);
  } else {
    // Update the <webview> attribute to match the BrowserPlugin attribute.
    // Note: Calling setAttribute on <webview> will trigger its mutation
    // observer which will then propagate that attribute to BrowserPlugin. In
    // cases where we permit assigning a BrowserPlugin attribute the same value
    // again (such as navigation when crashed), this could end up in an infinite
    // loop. Thus, we avoid this loop by only updating the <webview> attribute
    // if the BrowserPlugin attributes differs from it.
    var oldValue = this.webviewNode_.getAttribute(mutation.attributeName);
    var newValue = this.browserPluginNode_.getAttribute(mutation.attributeName);
    if (newValue != oldValue) {
      this.webviewNode_.setAttribute(mutation.attributeName, newValue);
    }
  }
};

/**
 * @private
 */
WebView.prototype.setupWebviewNodeEvents_ = function() {
  var self = this;
  var onInstanceIdAllocated = function(e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    self.instanceId_ = detail.windowId;
    var params = {
      'api': 'webview'
    };
    self.browserPluginNode_['-internal-attach'](params);

    for (var eventName in WEB_VIEW_EXT_EVENTS) {
      self.setupExtEvent_(eventName, WEB_VIEW_EXT_EVENTS[eventName]);
    }
  };
  this.browserPluginNode_.addEventListener('-internal-instanceid-allocated',
                                           onInstanceIdAllocated);

  for (var eventName in WEB_VIEW_EVENTS) {
    this.setupEvent_(eventName, WEB_VIEW_EVENTS[eventName]);
  }
  this.setupNewWindowEvent_();
  this.setupPermissionEvent_();
};

/**
 * @private
 */
WebView.prototype.setupExtEvent_ = function(eventName, eventInfo) {
  var self = this;
  var webviewNode = this.webviewNode_;
  eventInfo.evt.addListener(function(event) {
    var webviewEvent = new Event(eventName, {bubbles: true});
    $Array.forEach(eventInfo.fields, function(field) {
      webviewEvent[field] = event[field];
    });
    if (eventInfo.customHandler) {
      eventInfo.customHandler(self, event);
    }
    webviewNode.dispatchEvent(webviewEvent);
  }, {instanceId: self.instanceId_});
};

/**
 * @private
 */
WebView.prototype.setupEvent_ = function(eventName, attribs) {
  var webviewNode = this.webviewNode_;
  var internalname = '-internal-' + eventName;
  this.browserPluginNode_.addEventListener(internalname, function(e) {
    var evt = new Event(eventName, { bubbles: true });
    var detail = e.detail ? JSON.parse(e.detail) : {};
    $Array.forEach(attribs, function(attribName) {
      evt[attribName] = detail[attribName];
    });
    webviewNode.dispatchEvent(evt);
  });
};

/**
 * @private
 */
WebView.prototype.setupNewWindowEvent_ = function() {
  var ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN = '<webview>: ' +
      'An action has already been taken for this "newwindow" event.';

  var ERROR_MSG_NEWWINDOW_UNABLE_TO_ATTACH = '<webview>: ' +
      'Unable to attach the new window to the provided webview.';

  var ERROR_MSG_WEBVIEW_EXPECTED = '<webview> element expected.';

  var WARNING_MSG_NEWWINDOW_BLOCKED = '<webview>: A new window was blocked.';

  var NEW_WINDOW_EVENT_ATTRIBUTES = [
    'initialHeight',
    'initialWidth',
    'targetUrl',
    'windowOpenDisposition',
    'name'
  ];

  var self = this;
  var node = this.webviewNode_;
  var browserPluginNode = this.browserPluginNode_;

  var onTrackedObjectGone = function(requestId, e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    if (detail.id != requestId)
      return;
    browserPluginNode['-internal-setPermission'](requestId, false, '');
  }

  browserPluginNode.addEventListener('-internal-newwindow', function(e) {
    var evt = new Event('newwindow', { bubbles: true, cancelable: true });
    var detail = e.detail ? JSON.parse(e.detail) : {};

    $Array.forEach(NEW_WINDOW_EVENT_ATTRIBUTES, function(attribName) {
      evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;
    var actionTaken = false;

    var validateCall = function () {
      if (actionTaken)
        throw new Error(ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN);
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
                                                            detail.windowId);
          if (!attached) {
            console.error(ERROR_MSG_NEWWINDOW_UNABLE_TO_ATTACH);
          }
          // If the object being passed into attach is not a valid <webview>
          // then we will fail and it will be treated as if the new window
          // was rejected. The permission API plumbing is used here to clean
          // up the state created for the new window if attaching fails.
          browserPluginNode['-internal-setPermission'](requestId, attached, '');
        }, 0);
      },
      discard: function() {
        validateCall();
        browserPluginNode['-internal-setPermission'](requestId, false, '');
      }
    };
    evt.window = window;
    // Make browser plugin track lifetime of |window|.
    var onTrackedObjectGoneWithRequestId =
        $Function.bind(onTrackedObjectGone, self, requestId);
    browserPluginNode.addEventListener('-internal-trackedobjectgone',
        onTrackedObjectGoneWithRequestId);
    browserPluginNode['-internal-trackObjectLifetime'](window, requestId);

    var defaultPrevented = !node.dispatchEvent(evt);
    if (!actionTaken && !defaultPrevented) {
      actionTaken = true;
      // The default action is to discard the window.
      browserPluginNode['-internal-setPermission'](requestId, false, '');
      console.warn(WARNING_MSG_NEWWINDOW_BLOCKED);
    }
  });
};

/**
 * @private
 */
WebView.prototype.setupExecuteCodeAPI_ = function() {
  var ERROR_MSG_CANNOT_INJECT_SCRIPT = '<webview>: ' +
      'Script cannot be injected into content until the page has loaded.';

  var self = this;
  var validateCall = function() {
    if (!self.browserPluginNode_.getGuestInstanceId()) {
      throw new Error(ERROR_MSG_CANNOT_INJECT_SCRIPT);
    }
  };

  this.webviewNode_['executeScript'] = function(var_args) {
    validateCall();
    var args = $Array.concat([self.browserPluginNode_.getGuestInstanceId()],
                             $Array.slice(arguments));
    $Function.apply(chrome.webview.executeScript, null, args);
  }
  this.webviewNode_['insertCSS'] = function(var_args) {
    validateCall();
    var args = $Array.concat([self.browserPluginNode_.getGuestInstanceId()],
                             $Array.slice(arguments));
    $Function.apply(chrome.webview.insertCSS, null, args);
  }
};

/**
 * @private
 */
WebView.prototype.getPermissionTypes_ = function() {
  var PERMISSION_TYPES = ['media', 'geolocation', 'pointerLock'];
  return PERMISSION_TYPES.concat(this.maybeGetExperimentalPermissionTypes_());
};

/**
 * @param {!Object} detail The event details, originated from <object>.
 * @private
 */
WebView.prototype.setupPermissionEvent_ = function() {
  var PERMISSION_TYPES = this.getPermissionTypes_();

  var EXPOSED_PERMISSION_EVENT_ATTRIBS = [
      'lastUnlockedBySelf',
      'permission',
      'requestMethod',
      'url',
      'userGesture'
  ];

  var ERROR_MSG_PERMISSION_ALREADY_DECIDED = '<webview>: ' +
      'Permission has already been decided for this "permissionrequest" event.';

  var self = this;
  var node = this.webviewNode_;
  var browserPluginNode = this.browserPluginNode_;
  var internalevent = '-internal-permissionrequest';

  var onTrackedObjectGone = function(requestId, e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    if (detail.id != requestId)
      return;
    browserPluginNode['-internal-setPermission'](requestId, false, '');
  }

  browserPluginNode.addEventListener(internalevent, function(e) {
    var evt = new Event('permissionrequest', {bubbles: true, cancelable: true});
    var detail = e.detail ? JSON.parse(e.detail) : {};
    $Array.forEach(EXPOSED_PERMISSION_EVENT_ATTRIBS, function(attribName) {
      if (detail[attribName] !== undefined)
        evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;

    if (detail.requestId !== undefined &&
        PERMISSION_TYPES.indexOf(detail.permission) >= 0) {
      // TODO(lazyboy): Also fill in evt.details (see webview specs).
      // http://crbug.com/141197.
      var decisionMade = false;
      // Construct the event.request object.
      var request = {
        allow: function() {
          if (decisionMade) {
            throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
          } else {
            browserPluginNode['-internal-setPermission'](requestId, true, '');
            decisionMade = true;
          }
        },
        deny: function() {
          if (decisionMade) {
            throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
          } else {
            browserPluginNode['-internal-setPermission'](requestId, false, '');
            decisionMade = true;
          }
        }
      };
      evt.request = request;

      // Make browser plugin track lifetime of |request|.
      var onTrackedObjectGoneWithRequestId =
          $Function.bind(onTrackedObjectGone, self, requestId);
      browserPluginNode.addEventListener('-internal-trackedobjectgone',
          onTrackedObjectGoneWithRequestId);
      browserPluginNode['-internal-trackObjectLifetime'](request, requestId);

      var defaultPrevented = !node.dispatchEvent(evt);
      if (!decisionMade && !defaultPrevented) {
        decisionMade = true;
        browserPluginNode['-internal-setPermission'](requestId, false, '');
      }
    }
  });
};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebView.prototype.maybeGetExperimentalPermissionTypes_ = function() {
  return [];
};

/**
 * Implemented when the experimental API is available.
 * @private
 */
WebView.prototype.maybeSetupExperimentalAPI_ = function() {};

exports.WebView = WebView;
