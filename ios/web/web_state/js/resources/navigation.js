// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Navigation related APIs.
 */

goog.provide('__crWeb.navigation');

/** Beginning of anonymouse object */
(function() {

  /**
   * A popstate event needs to be fired anytime the active history entry
   * changes without an associated document change. Either via back, forward, go
   * navigation or by loading the URL, clicking on a link, etc.
   */
  __gCrWeb['dispatchPopstateEvent'] = function(stateObject) {
    var popstateEvent = window.document.createEvent('HTMLEvents');
    popstateEvent.initEvent('popstate', true, false);
    if (stateObject)
      popstateEvent.state = JSON.parse(stateObject);

    // setTimeout() is used in order to return immediately. Otherwise the
    // dispatchEvent call waits for all event handlers to return, which could
    // cause a ReentryGuard failure.
    window.setTimeout(function() {
      window.dispatchEvent(popstateEvent);
    }, 0);
  };

  /**
   * A hashchange event needs to be fired after a same-document history
   * navigation between two URLs that are equivalent except for their fragments.
   */
  __gCrWeb['dispatchHashchangeEvent'] = function(oldURL, newURL) {
    var hashchangeEvent = window.document.createEvent('HTMLEvents');
    hashchangeEvent.initEvent('hashchange', true, false);
    if (oldURL)
      hashchangeEvent.oldURL = oldURL;
    if (newURL)
      hashchangeEvent.newURL = newURL

    // setTimeout() is used in order to return immediately. Otherwise the
    // dispatchEvent call waits for all event handlers to return, which could
    // cause a ReentryGuard failure.
    window.setTimeout(function() {
      window.dispatchEvent(hashchangeEvent);
    }, 0);
  };

  /**
   * Keep the original pushState() and replaceState() methods. It's needed to
   * update the web view's URL and window.history.state property during history
   * navigations that don't cause a page load.
   * @private
   */
  var originalWindowHistoryPushState = window.history.pushState;
  var originalWindowHistoryReplaceState = window.history.replaceState;

  __gCrWeb['replaceWebViewURL'] = function(url, stateObject) {
    originalWindowHistoryReplaceState.call(history, stateObject, '', url);
  };

  /**
   * Intercept window.history methods to call back/forward natively.
   */
  window.history.back = function() {
    __gCrWeb.message.invokeOnHost({'command': 'window.history.back'});
  };

  window.history.forward = function() {
    __gCrWeb.message.invokeOnHost({'command': 'window.history.forward'});
  };

  window.history.go = function(delta) {
    __gCrWeb.message.invokeOnHost(
        {'command': 'window.history.go', 'value': delta | 0});
  };

  window.history.pushState = function(stateObject, pageTitle, pageUrl) {
    __gCrWeb.message.invokeOnHost(
        {'command': 'window.history.willChangeState'});
    // Calling stringify() on undefined causes a JSON parse error.
    var serializedState =
        typeof(stateObject) == 'undefined' ? '' :
            __gCrWeb.common.JSONStringify(stateObject);
    pageUrl = pageUrl || window.location.href;
    originalWindowHistoryPushState.call(history, stateObject,
                                        pageTitle, pageUrl);
    __gCrWeb.message.invokeOnHost(
        {'command': 'window.history.didPushState',
         'stateObject': serializedState,
         'baseUrl': document.baseURI,
         'pageUrl': pageUrl.toString()});
  };

  window.history.replaceState = function(stateObject, pageTitle, pageUrl) {
    __gCrWeb.message.invokeOnHost(
        {'command': 'window.history.willChangeState'});

    // Calling stringify() on undefined causes a JSON parse error.
    var serializedState =
        typeof(stateObject) == 'undefined' ? '' :
            __gCrWeb.common.JSONStringify(stateObject);
    pageUrl = pageUrl || window.location.href;
    originalWindowHistoryReplaceState.call(history, stateObject,
                                           pageTitle, pageUrl);
    __gCrWeb.message.invokeOnHost(
        {'command': 'window.history.didReplaceState',
         'stateObject': serializedState,
         'baseUrl': document.baseURI,
         'pageUrl': pageUrl.toString()});
  };

  window.addEventListener('hashchange', function(evt) {
    // Because hash changes don't trigger __gCrWeb.didFinishNavigation, so fetch
    // favicons for the new page manually.
    __gCrWeb.message.invokeOnHost({'command': 'document.favicons',
                                   'favicons': __gCrWeb.common.getFavicons()});

    __gCrWeb.message.invokeOnHost({'command': 'window.hashchange'});
  });

  /** Flush the message queue. */
  if (__gCrWeb.message) {
    __gCrWeb.message.invokeQueues();
  }

}());  // End of anonymouse object
