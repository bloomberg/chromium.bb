// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts that are conceptually part of core.js, but have WKWebView-specific
// details/behaviors.

goog.provide('__crWeb.coreDynamic');

/**
 * Namespace for this module.
 */
__gCrWeb.core_dynamic = {};

/* Beginning of anonymous object. */
(function() {
  /**
   * Adds WKWebView specific event listeners.
   */
  __gCrWeb.core_dynamic.addEventListeners = function() {
    // So far there are no WKWebView specific event listeners.
  };

  /**
   * Applies WKWebView specific document-level overrides. Script injection for
   * WKWebView always happens after the document is presented; therefore, there
   * is no need to provide for invoking documentInject at a later time.
   */
  __gCrWeb.core_dynamic.documentInject = function() {
    // Flush the message queue.
    if (__gCrWeb.message) {
      __gCrWeb.message.invokeQueues();
    }
    return true;
  };

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
}());
