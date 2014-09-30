// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <webview> API.

var ChromeWebView = require('chromeWebViewInternal').ChromeWebView;
var CreateEvent = require('webViewEvents').CreateEvent;
var DeclarativeWebRequestSchema =
    requireNative('schema_registry').GetSchema('declarativeWebRequest');
var EventBindings = require('event_bindings');
var IdGenerator = requireNative('id_generator');
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var WebRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebViewInternal = require('webView').WebViewInternal

var WebRequestMessageEvent = CreateEvent('webViewInternal.onMessage');

var CHROME_WEB_VIEW_EVENTS = {
  'contextmenu': {
    evt: CreateEvent('chromeWebViewInternal.contextmenu'),
    cancelable: true,
    customHandler: function(handler, event, webViewEvent) {
      handler.webViewInternal.maybeHandleContextMenu(event, webViewEvent);
    },
    fields: ['items']
  }
};

function DeclarativeWebRequestEvent(opt_eventName,
                                    opt_argSchemas,
                                    opt_eventOptions,
                                    opt_webViewInstanceId) {
  var subEventName = opt_eventName + '/' + IdGenerator.GetNextId();
  EventBindings.Event.call(this, subEventName, opt_argSchemas, opt_eventOptions,
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

/**
 * Implemented when the ChromeWebView API is available.
 * @private
 */
WebViewInternal.prototype.maybeGetChromeWebViewEvents = function() {
  return CHROME_WEB_VIEW_EVENTS;
};

/**
 * Calls to show contextmenu right away instead of dispatching a 'contextmenu'
 * event.
 * This will be overridden in chrome_web_view_experimental.js to implement
 * contextmenu  API.
 */
WebViewInternal.prototype.maybeHandleContextMenu = function(e, webViewEvent) {
  var requestId = e.requestId;
  // Setting |params| = undefined will show the context menu unmodified, hence
  // the 'contextmenu' API is disabled for stable channel.
  var params = undefined;
  ChromeWebView.showContextMenu(this.guestInstanceId, requestId, params);
};

WebViewInternal.prototype.maybeSetupChromeWebViewEvents = function() {
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
                'webViewInternal.' + webRequestEvent.name,
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

  this.setRequestPropertyOnWebViewNode(request);
};
