// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <webview> API.

var ChromeWebView = require('chromeWebViewInternal').ChromeWebView;
var CreateEvent = require('guestViewEvents').CreateEvent;
var EventBindings = require('event_bindings');
var WebViewEvents = require('webViewEvents').WebViewEvents;
var WebViewImpl = require('webView').WebViewImpl;

var CHROME_WEB_VIEW_EVENTS = {
  'contextmenu': {
    evt: CreateEvent('chromeWebViewInternal.contextmenu'),
    cancelable: true,
    customHandler: function(handler, event, webViewEvent) {
      handler.webViewImpl.maybeHandleContextMenu(event, webViewEvent);
    },
    fields: ['items']
  }
};

/**
 * Calls to show contextmenu right away instead of dispatching a 'contextmenu'
 * event.
 * This will be overridden in chrome_web_view_experimental.js to implement
 * contextmenu  API.
 */
WebViewImpl.prototype.maybeHandleContextMenu = function(e, webViewEvent) {
  var requestId = e.requestId;
  // Setting |params| = undefined will show the context menu unmodified, hence
  // the 'contextmenu' API is disabled for stable channel.
  var params = undefined;
  ChromeWebView.showContextMenu(this.guest.getId(), requestId, params);
};

// Exposes |CHROME_WEB_VIEW_EVENTS| when the ChromeWebView API is available.
(function() {
  for (var eventName in CHROME_WEB_VIEW_EVENTS) {
    WebViewEvents.EVENTS[eventName] = CHROME_WEB_VIEW_EVENTS[eventName];
  }
})();
