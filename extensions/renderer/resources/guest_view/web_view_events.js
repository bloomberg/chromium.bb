// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Event management for WebView.

var DeclarativeWebRequestSchema =
    requireNative('schema_registry').GetSchema('declarativeWebRequest');
var EventBindings = require('event_bindings');
var IdGenerator = requireNative('id_generator');
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var WebRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebViewActionRequests =
    require('webViewActionRequests').WebViewActionRequests;

var CreateEvent = function(name) {
  var eventOpts = {supportsListeners: true, supportsFilters: true};
  return new EventBindings.Event(name, undefined, eventOpts);
};

var FrameNameChangedEvent = CreateEvent('webViewInternal.onFrameNameChanged');
var PluginDestroyedEvent = CreateEvent('webViewInternal.onPluginDestroyed');
var WebRequestMessageEvent = CreateEvent('webViewInternal.onMessage');

// WEB_VIEW_EVENTS is a map of stable <webview> DOM event names to their
//     associated extension event descriptor objects.
// An event listener will be attached to the extension event |evt| specified in
//     the descriptor.
// |fields| specifies the public-facing fields in the DOM event that are
//     accessible to <webview> developers.
// |customHandler| allows a handler function to be called each time an extension
//     event is caught by its event listener. The DOM event should be dispatched
//     within this handler function. With no handler function, the DOM event
//     will be dispatched by default each time the extension event is caught.
// |cancelable| (default: false) specifies whether the event's default
//     behavior can be canceled. If the default action associated with the event
//     is prevented, then its dispatch function will return false in its event
//     handler. The event must have a custom handler for this to be meaningful.
var WEB_VIEW_EVENTS = {
  'close': {
    evt: CreateEvent('webViewInternal.onClose'),
    fields: []
  },
  'consolemessage': {
    evt: CreateEvent('webViewInternal.onConsoleMessage'),
    fields: ['level', 'message', 'line', 'sourceId']
  },
  'contentload': {
    evt: CreateEvent('webViewInternal.onContentLoad'),
    fields: []
  },
  'dialog': {
    cancelable: true,
    customHandler: function(handler, event, webViewEvent) {
      handler.handleDialogEvent(event, webViewEvent);
    },
    evt: CreateEvent('webViewInternal.onDialog'),
    fields: ['defaultPromptText', 'messageText', 'messageType', 'url']
  },
  'droplink': {
    evt: CreateEvent('webViewInternal.onDropLink'),
    fields: ['url']
  },
  'exit': {
    evt: CreateEvent('webViewInternal.onExit'),
    fields: ['processId', 'reason']
  },
  'findupdate': {
    evt: CreateEvent('webViewInternal.onFindReply'),
    fields: [
      'searchText',
      'numberOfMatches',
      'activeMatchOrdinal',
      'selectionRect',
      'canceled',
      'finalUpdate'
    ]
  },
  'loadabort': {
    cancelable: true,
    customHandler: function(handler, event, webViewEvent) {
      handler.handleLoadAbortEvent(event, webViewEvent);
    },
    evt: CreateEvent('webViewInternal.onLoadAbort'),
    fields: ['url', 'isTopLevel', 'reason']
  },
  'loadcommit': {
    customHandler: function(handler, event, webViewEvent) {
      handler.handleLoadCommitEvent(event, webViewEvent);
    },
    evt: CreateEvent('webViewInternal.onLoadCommit'),
    fields: ['url', 'isTopLevel']
  },
  'loadprogress': {
    evt: CreateEvent('webViewInternal.onLoadProgress'),
    fields: ['url', 'progress']
  },
  'loadredirect': {
    evt: CreateEvent('webViewInternal.onLoadRedirect'),
    fields: ['isTopLevel', 'oldUrl', 'newUrl']
  },
  'loadstart': {
    evt: CreateEvent('webViewInternal.onLoadStart'),
    fields: ['url', 'isTopLevel']
  },
  'loadstop': {
    evt: CreateEvent('webViewInternal.onLoadStop'),
    fields: []
  },
  'newwindow': {
    cancelable: true,
    customHandler: function(handler, event, webViewEvent) {
      handler.handleNewWindowEvent(event, webViewEvent);
    },
    evt: CreateEvent('webViewInternal.onNewWindow'),
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
    customHandler: function(handler, event, webViewEvent) {
      handler.handlePermissionEvent(event, webViewEvent);
    },
    evt: CreateEvent('webViewInternal.onPermissionRequest'),
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
    evt: CreateEvent('webViewInternal.onResponsive'),
    fields: ['processId']
  },
  'sizechanged': {
    evt: CreateEvent('webViewInternal.onSizeChanged'),
    customHandler: function(handler, event, webViewEvent) {
      handler.handleSizeChangedEvent(event, webViewEvent);
    },
    fields: ['oldHeight', 'oldWidth', 'newHeight', 'newWidth']
  },
  'unresponsive': {
    evt: CreateEvent('webViewInternal.onUnresponsive'),
    fields: ['processId']
  },
  'zoomchange': {
    evt: CreateEvent('webViewInternal.onZoomChange'),
    fields: ['oldZoomFactor', 'newZoomFactor']
  }
};

function DeclarativeWebRequestEvent(opt_eventName,
                                    opt_argSchemas,
                                    opt_eventOptions,
                                    opt_webViewInstanceId) {
  var subEventName = opt_eventName + '/' + IdGenerator.GetNextId();
  EventBindings.Event.call(this,
                           subEventName,
                           opt_argSchemas,
                           opt_eventOptions,
                           opt_webViewInstanceId);

  // TODO(lazyboy): When do we dispose this listener?
  WebRequestMessageEvent.addListener(function() {
    // Re-dispatch to subEvent's listeners.
    $Function.apply(this.dispatch, this, $Array.slice(arguments));
  }.bind(this), {instanceId: opt_webViewInstanceId || 0});
}

DeclarativeWebRequestEvent.prototype = {
  __proto__: EventBindings.Event.prototype
};

// Constructor.
function WebViewEvents(webViewImpl, viewInstanceId) {
  this.webViewImpl = webViewImpl;
  this.viewInstanceId = viewInstanceId;

  // on* Event handlers.
  this.on = {};

  // Set up the events.
  this.setupFrameNameChangedEvent();
  this.setupWebRequestEvents();
  this.webViewImpl.setupExperimentalContextMenus();
  var events = this.getEvents();
  for (var eventName in events) {
    this.setupEvent(eventName, events[eventName]);
  }
}

WebViewEvents.prototype.setupFrameNameChangedEvent = function() {
  FrameNameChangedEvent.addListener(function(e) {
    this.webViewImpl.onFrameNameChanged(e.name);
  }.bind(this), {instanceId: this.viewInstanceId});
};

WebViewEvents.prototype.setupWebRequestEvents = function() {
  var request = {};
  var createWebRequestEvent = function(webRequestEvent) {
    return function() {
      if (!this[webRequestEvent.name]) {
        this[webRequestEvent.name] =
            new WebRequestEvent(
                'webViewInternal.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.extraParameters, webRequestEvent.options,
                this.viewInstanceId);
      }
      return this[webRequestEvent.name];
    }.bind(this);
  }.bind(this);

  var createDeclarativeWebRequestEvent = function(webRequestEvent) {
    return function() {
      if (!this[webRequestEvent.name]) {
        // The onMessage event gets a special event type because we want
        // the listener to fire only for messages targeted for this particular
        // <webview>.
        var EventClass = webRequestEvent.name === 'onMessage' ?
            DeclarativeWebRequestEvent : EventBindings.Event;
        this[webRequestEvent.name] =
            new EventClass(
                'webViewInternal.declarativeWebRequest.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.options,
                this.viewInstanceId);
      }
      return this[webRequestEvent.name];
    }.bind(this);
  }.bind(this);

  for (var i = 0; i < DeclarativeWebRequestSchema.events.length; ++i) {
    var eventSchema = DeclarativeWebRequestSchema.events[i];
    var webRequestEvent = createDeclarativeWebRequestEvent(eventSchema);
    Object.defineProperty(
        request,
        eventSchema.name,
        {
          get: webRequestEvent,
          enumerable: true
        }
    );
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
  }

  this.webViewImpl.setRequestPropertyOnWebViewElement(request);
};

WebViewEvents.prototype.getEvents = function() {
  var chromeEvents = this.webViewImpl.maybeGetChromeWebViewEvents();
  for (var eventName in chromeEvents) {
    WEB_VIEW_EVENTS[eventName] = chromeEvents[eventName];
  }
  return WEB_VIEW_EVENTS;
};

WebViewEvents.prototype.setupEvent = function(name, info) {
  info.evt.addListener(function(e) {
    var details = {bubbles: true};
    if (info.cancelable) {
      details.cancelable = true;
    }
    var webViewEvent = new Event(name, details);
    $Array.forEach(info.fields, function(field) {
      if (e[field] !== undefined) {
        webViewEvent[field] = e[field];
      }
    }.bind(this));
    if (info.customHandler) {
      info.customHandler(this, e, webViewEvent);
      return;
    }
    this.webViewImpl.dispatchEvent(webViewEvent);
  }.bind(this), {instanceId: this.viewInstanceId});

  this.setupEventProperty(name);
};

WebViewEvents.prototype.setupEventProperty = function(eventName) {
  var propertyName = 'on' + eventName.toLowerCase();
  Object.defineProperty(this.webViewImpl.element, propertyName, {
    get: function() {
      return this.on[propertyName];
    }.bind(this),
    set: function(value) {
      if (this.on[propertyName]) {
        this.webViewImpl.element.removeEventListener(
            eventName, this.on[propertyName]);
      }
      this.on[propertyName] = value;
      if (value)
        this.webViewImpl.element.addEventListener(eventName, value);
    }.bind(this),
    enumerable: true
  });
};

WebViewEvents.prototype.handleDialogEvent = function(event, webViewEvent) {
  new WebViewActionRequests.Dialog(this.webViewImpl, event, webViewEvent);
};

WebViewEvents.prototype.handleLoadAbortEvent = function(event, webViewEvent) {
  var showWarningMessage = function(reason) {
    var WARNING_MSG_LOAD_ABORTED = '<webview>: ' +
        'The load has aborted with reason "%1".';
    window.console.warn(WARNING_MSG_LOAD_ABORTED.replace('%1', reason));
  };
  if (this.webViewImpl.dispatchEvent(webViewEvent)) {
    showWarningMessage(event.reason);
  }
};

WebViewEvents.prototype.handleLoadCommitEvent = function(event, webViewEvent) {
  this.webViewImpl.onLoadCommit(event.baseUrlForDataUrl,
                                event.currentEntryIndex,
                                event.entryCount,
                                event.processId,
                                event.url,
                                event.isTopLevel);
  this.webViewImpl.dispatchEvent(webViewEvent);
};

WebViewEvents.prototype.handleNewWindowEvent = function(event, webViewEvent) {
  new WebViewActionRequests.NewWindow(this.webViewImpl, event, webViewEvent);
};

WebViewEvents.prototype.handlePermissionEvent = function(event, webViewEvent) {
  new WebViewActionRequests.PermissionRequest(
      this.webViewImpl, event, webViewEvent);
};

WebViewEvents.prototype.handleSizeChangedEvent = function(
    event, webViewEvent) {
  this.webViewImpl.onSizeChanged(webViewEvent);
};

exports.WebViewEvents = WebViewEvents;
exports.CreateEvent = CreateEvent;
