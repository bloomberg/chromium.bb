// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Collection of legacy APIs that should eventually be cleaned up.
 */

goog.provide('__crWeb.legacy');

goog.require('__crWeb.common');
goog.require('__crWeb.message');

/** Beginning of anonymouse object */
(function() {

  /**
   * Handles document load completion tasks. Invoked from
   * [WKNavigationDelegate webView:didFinishNavigation:], when document load is
   * complete.
   * TODO(crbug.com/546350): Investigate using
   * WKUserScriptInjectionTimeAtDocumentEnd to inject this material at the
   * appropriate time so that this API will not be needed.
   */
  __gCrWeb.didFinishNavigation = function() {
    // Send the favicons to the browser.
    __gCrWeb.message.invokeOnHost({'command': 'document.favicons',
                                   'favicons': __gCrWeb.common.getFavicons()});

    // Add placeholders for plugin content.
    if (__gCrWeb.common.updatePluginPlaceholders())
      __gCrWeb.message.invokeOnHost({'command': 'addPluginPlaceholders'});
  }

}());  // End of anonymouse object
