// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adheres to closure-compiler conventions in order to enable
// compilation with ADVANCED_OPTIMIZATIONS. In particular, members that are to
// be accessed externally should be specified in this['style'] as opposed to
// this.style because member identifiers are minified by default.
// See http://goo.gl/FwOgy

goog.provide('__crWeb.core');

goog.require('__crWeb.common');
goog.require('__crWeb.message');

/* Beginning of anonymous object. */
(function() {
  __gCrWeb['core'] = {};

  /**
   * Handles document load completion tasks. Invoked from
   * [WKNavigationDelegate webView:didFinishNavigation:], when document load is
   * complete.
   */
  __gCrWeb.didFinishNavigation = function() {
    // Send the favicons to the browser.
    __gCrWeb.sendFaviconsToHost();
    // Add placeholders for plugin content.
    if (__gCrWeb.common.updatePluginPlaceholders())
      __gCrWeb.message.invokeOnHost({'command': 'addPluginPlaceholders'});
  }

  // JavaScript errors are logged on the main application side. The handler is
  // added ASAP to catch any errors in startup. Note this does not appear to
  // work in iOS < 5.
  window.addEventListener('error', function(event) {
    // Sadly, event.filename and event.lineno are always 'undefined' and '0'
    // with UIWebView.
    __gCrWeb.message.invokeOnHost(
        {'command': 'window.error', 'message': event.message.toString()});
  });

  __gCrWeb['sendFaviconsToHost'] = function() {
    __gCrWeb.message.invokeOnHost({'command': 'document.favicons',
                                   'favicons': __gCrWeb.common.getFavicons()});
  }

  // Flush the message queue.
  if (__gCrWeb.message) {
    __gCrWeb.message.invokeQueues();
  }

}());  // End of anonymous object
