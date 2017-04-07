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
    invokeOnHost_({'command': 'window.error',
                   'message': event.message.toString()});
  });


  // Returns true if the top window or any frames inside contain an input
  // field of type 'password'.
  __gCrWeb['hasPasswordField'] = function() {
    return hasPasswordField_(window);
  };

  // Returns a string that is formatted according to the JSON syntax rules.
  // This is equivalent to the built-in JSON.stringify() function, but is
  // less likely to be overridden by the website itself.  This public function
  // should not be used if spoofing it would create a security vulnerability.
  // The |__gCrWeb| object itself does not use it; it uses its private
  // counterpart instead.
  // Prevents websites from changing stringify's behavior by adding the
  // method toJSON() by temporarily removing it.
  __gCrWeb['stringify'] = function(value) {
    if (value === null)
      return 'null';
    if (value === undefined)
      return undefined;
    if (typeof(value.toJSON) == 'function') {
      var originalToJSON = value.toJSON;
      value.toJSON = undefined;
      var stringifiedValue = __gCrWeb.common.JSONStringify(value);
      value.toJSON = originalToJSON;
      return stringifiedValue;
    }
    return __gCrWeb.common.JSONStringify(value);
  };

  /*
   * Adds the listeners that are used to handle forms, enabling autofill and
   * the replacement method to dismiss the keyboard needed because of the
   * Autofill keyboard accessory.
   */
  function addFormEventListeners_() {
    // Focus and input events for form elements are messaged to the main
    // application for broadcast to CRWWebControllerObservers.
    // This is done with a single event handler for each type being added to the
    // main document element which checks the source element of the event; this
    // is much easier to manage than adding handlers to individual elements.
    var formActivity = function(evt) {
      var srcElement = evt.srcElement;
      var fieldName = srcElement.name || '';
      var value = srcElement.value || '';

      var msg = {
        'command': 'form.activity',
        'formName': __gCrWeb.common.getFormIdentifier(evt.srcElement.form),
        'fieldName': fieldName,
        'type': evt.type,
        'value': value
      };
      invokeOnHost_(msg);
    };

    // Focus events performed on the 'capture' phase otherwise they are often
    // not received.
    document.addEventListener('focus', formActivity, true);
    document.addEventListener('blur', formActivity, true);
    document.addEventListener('change', formActivity, true);

    // Text input is watched at the bubbling phase as this seems adequate in
    // practice and it is less obtrusive to page scripts than capture phase.
    document.addEventListener('input', formActivity, false);
    document.addEventListener('keyup', formActivity, false);
  };

  // Returns true if the supplied window or any frames inside contain an input
  // field of type 'password'.
  // @private
  var hasPasswordField_ = function(win) {
    var doc = win.document;

    // We may will not be allowed to read the 'document' property from a frame
    // that is in a different domain.
    if (!doc) {
      return false;
    }

    if (doc.querySelector('input[type=password]')) {
      return true;
    }

    var frames = win.frames;
    for (var i = 0; i < frames.length; i++) {
      if (hasPasswordField_(frames[i])) {
        return true;
      }
    }

    return false;
  };

  function invokeOnHost_(command) {
    __gCrWeb.message.invokeOnHost(command);
  };

  // Various aspects of global DOM behavior are overridden here.

  // A popstate event needs to be fired anytime the active history entry
  // changes without an associated document change. Either via back, forward, go
  // navigation or by loading the URL, clicking on a link, etc.
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

  // A hashchange event needs to be fired after a same-document history
  // navigation between two URLs that are equivalent except for their fragments.
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

  // Keep the original pushState() and replaceState() methods. It's needed to
  // update the web view's URL and window.history.state property during history
  // navigations that don't cause a page load.
  var originalWindowHistoryPushState = window.history.pushState;
  var originalWindowHistoryReplaceState = window.history.replaceState;
  __gCrWeb['replaceWebViewURL'] = function(url, stateObject) {
    originalWindowHistoryReplaceState.call(history, stateObject, '', url);
  };

  // Intercept window.history methods to call back/forward natively.
  window.history.back = function() {
    invokeOnHost_({'command': 'window.history.back'});
  };
  window.history.forward = function() {
    invokeOnHost_({'command': 'window.history.forward'});
  };
  window.history.go = function(delta) {
    invokeOnHost_({'command': 'window.history.go', 'value': delta | 0});
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
    invokeOnHost_({'command': 'window.history.didPushState',
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
    invokeOnHost_({'command': 'window.history.didReplaceState',
                   'stateObject': serializedState,
                   'baseUrl': document.baseURI,
                   'pageUrl': pageUrl.toString()});
  };

  __gCrWeb['getFullyQualifiedURL'] = function(originalURL) {
    // A dummy anchor (never added to the document) is used to obtain the
    // fully-qualified URL of |originalURL|.
    var anchor = document.createElement('a');
    anchor.href = originalURL;
    return anchor.href;
  };

  __gCrWeb['sendFaviconsToHost'] = function() {
    __gCrWeb.message.invokeOnHost({'command': 'document.favicons',
                                   'favicons': __gCrWeb.common.getFavicons()});
  }

  // Tracks whether user is in the middle of scrolling/dragging. If user is
  // scrolling, ignore window.scrollTo() until user stops scrolling.
  var webViewScrollViewIsDragging_ = false;
  __gCrWeb['setWebViewScrollViewIsDragging'] = function(state) {
    webViewScrollViewIsDragging_ = state;
  };
  var originalWindowScrollTo = window.scrollTo;
  window.scrollTo = function(x, y) {
    if (webViewScrollViewIsDragging_)
      return;
    originalWindowScrollTo(x, y);
  };

  window.addEventListener('hashchange', function(evt) {
    invokeOnHost_({'command': 'window.hashchange'});
  });

  // Flush the message queue.
  if (__gCrWeb.message) {
    __gCrWeb.message.invokeQueues();
  }

  // Capture form submit actions.
  document.addEventListener('submit', function(evt) {
    var action;
    if (evt['defaultPrevented'])
      return;
    action = evt.target.getAttribute('action');
    // Default action is to re-submit to same page.
    if (!action)
      action = document.location.href;
    invokeOnHost_({
             'command': 'document.submit',
            'formName': __gCrWeb.common.getFormIdentifier(evt.srcElement),
                'href': __gCrWeb['getFullyQualifiedURL'](action)
    });
  }, false);

  addFormEventListeners_();

}());  // End of anonymous object
