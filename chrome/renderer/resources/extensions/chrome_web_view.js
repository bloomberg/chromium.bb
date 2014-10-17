// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <webview> API.

var ChromeWebView = require('chromeWebViewInternal').ChromeWebView;
var CreateEvent = require('webViewEvents').CreateEvent;
var EventBindings = require('event_bindings');
var WebView = require('webView').WebView;

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

/**
 * Implemented when the ChromeWebView API is available.
 * @private
 */
WebView.prototype.maybeGetChromeWebViewEvents = function() {
  return CHROME_WEB_VIEW_EVENTS;
};

/**
 * Calls to show contextmenu right away instead of dispatching a 'contextmenu'
 * event.
 * This will be overridden in chrome_web_view_experimental.js to implement
 * contextmenu  API.
 */
WebView.prototype.maybeHandleContextMenu = function(e, webViewEvent) {
  var requestId = e.requestId;
  // Setting |params| = undefined will show the context menu unmodified, hence
  // the 'contextmenu' API is disabled for stable channel.
  var params = undefined;
  ChromeWebView.showContextMenu(this.guestInstanceId, requestId, params);
};
