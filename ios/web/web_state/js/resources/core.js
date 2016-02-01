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
goog.require('__crWeb.coreDynamic');
goog.require('__crWeb.message');

/**
 * The Chrome object is populated in an anonymous object defined at
 * initialization to prevent polluting the global namespace.
 */

/* Beginning of anonymous object. */
(function() {
  // TODO(jimblackler): use this namespace as a wrapper for all externally-
  // visible functions, to be consistent with other JS scripts. crbug.com/380390
  __gCrWeb['core'] = {};

  // JavaScript errors are logged on the main application side. The handler is
  // added ASAP to catch any errors in startup. Note this does not appear to
  // work in iOS < 5.
  window.addEventListener('error', function(event) {
    // Sadly, event.filename and event.lineno are always 'undefined' and '0'
    // with UIWebView.
    invokeOnHost_({'command': 'window.error',
                   'message': event.message.toString()});
  });

  /**
   * Margin in points around touchable elements (e.g. links for custom context
   * menu).
   * @type {number}
   */
  var touchMargin_ = 25;

  __gCrWeb['getPageWidth'] = function() {
    var documentElement = document.documentElement;
    var documentBody = document.body;
    return Math.max(documentElement.clientWidth,
                    documentElement.scrollWidth,
                    documentElement.offsetWidth,
                    documentBody.scrollWidth,
                    documentBody.offsetWidth);
  };

  // Implementation of document.elementFromPoint that is working for iOS4 and
  // iOS5 and that also goes into frames and iframes.
  var elementFromPoint_ = function(x, y) {
    var elementFromPointIsUsingViewPortCoordinates = function(win) {
      if (win.pageYOffset > 0) {  // Page scrolled down.
        return (win.document.elementFromPoint(
            0, win.pageYOffset + win.innerHeight - 1) === null);
      }
      if (win.pageXOffset > 0) {  // Page scrolled to the right.
        return (win.document.elementFromPoint(
            win.pageXOffset + win.innerWidth - 1, 0) === null);
      }
      return false;  // No scrolling, don't care.
    };

    var newCoordinate = function(x, y) {
      var coordinates = {
          x: x, y: y,
          viewPortX: x - window.pageXOffset, viewPortY: y - window.pageYOffset,
          useViewPortCoordinates: false,
          window: window
      };
      return coordinates;
    };

    // Returns the coordinates of the upper left corner of |obj| in the
    // coordinates of the window that |obj| is in.
    var getPositionInWindow = function(obj) {
      var coord = { x: 0, y: 0 };
      while (obj.offsetParent) {
        coord.x += obj.offsetLeft;
        coord.y += obj.offsetTop;
        obj = obj.offsetParent;
      }
      return coord;
    };

    var elementsFromCoordinates = function(coordinates) {
      coordinates.useViewPortCoordinates = coordinates.useViewPortCoordinates ||
          elementFromPointIsUsingViewPortCoordinates(coordinates.window);

      var currentElement = null;
      if (coordinates.useViewPortCoordinates) {
        currentElement = coordinates.window.document.elementFromPoint(
            coordinates.viewPortX, coordinates.viewPortY);
      } else {
        currentElement = coordinates.window.document.elementFromPoint(
            coordinates.x, coordinates.y);
      }
      // We have to check for tagName, because if a selection is made by the
      // UIWebView, the element we will get won't have one.
      if (!currentElement || !currentElement.tagName) {
        return null;
      }
      if (currentElement.tagName.toLowerCase() === 'iframe' ||
          currentElement.tagName.toLowerCase() === 'frame') {
        // The following condition is true if the iframe is in a different
        // domain; no further information is accessible.
        if (typeof(currentElement.contentWindow.document) == 'undefined') {
          return currentElement;
        }
        var framePosition = getPositionInWindow(currentElement);
        coordinates.viewPortX -=
            framePosition.x - coordinates.window.pageXOffset;
        coordinates.viewPortY -=
            framePosition.y - coordinates.window.pageYOffset;
        coordinates.window = currentElement.contentWindow;
        coordinates.x -= framePosition.x + coordinates.window.pageXOffset;
        coordinates.y -= framePosition.y + coordinates.window.pageYOffset;
        return elementsFromCoordinates(coordinates);
      }
      return currentElement;
    };

    return elementsFromCoordinates(newCoordinate(x, y));
  };

  var spiralCoordinates = function(x, y) {
    var coordinates = [];

    var maxAngle = Math.PI * 2.0 * 3.0;
    var pointCount = 30;
    var angleStep = maxAngle / pointCount;
    var speed = touchMargin_ / maxAngle;

    for (var index = 0; index < pointCount; index++) {
      var angle = angleStep * index;
      var radius = angle * speed;

      coordinates.push({x: x + Math.round(Math.cos(angle) * radius),
                        y: y + Math.round(Math.sin(angle) * radius)});
    }

    return coordinates;
  };

  // Returns the url of the image or link under the selected point. Returns an
  // empty string if no links or images are found.
  __gCrWeb['getElementFromPoint'] = function(x, y) {
    var hitCoordinates = spiralCoordinates(x, y);
    for (var index = 0; index < hitCoordinates.length; index++) {
      var coordinates = hitCoordinates[index];

      var element = elementFromPoint_(coordinates.x, coordinates.y);
      if (!element || !element.tagName) {
        // Nothing under the hit point. Try the next hit point.
        continue;
      }

      if (getComputedWebkitTouchCallout_(element) === 'none')
        continue;
      // Also check element's ancestors. A bound on the level is used here to
      // avoid large overhead when no links or images are found.
      var level = 0;
      while (++level < 8 && element && element != document) {
        var tagName = element.tagName;
        if (!tagName)
          continue;
        tagName = tagName.toLowerCase();

        if (tagName === 'input' || tagName === 'textarea' ||
            tagName === 'select' || tagName === 'option') {
          // If the element is a known input element, stop the spiral search and
          // return empty results.
          return '{}';
        }

        if (tagName === 'a' && element.href) {
          // Found a link.
          return __gCrWeb.common.JSONStringify(
              {href: element.href,
               referrerPolicy: getReferrerPolicy_(element)});
        }

        if (tagName === 'img' && element.src) {
          // Found an image.
          var result = {src: element.src,
                        referrerPolicy: getReferrerPolicy_()};
          // Copy the title, if any.
          if (element.title) {
            result.title = element.title;
          }
          // Check if the image is also a link.
          var parent = element.parentNode;
          while (parent) {
            if (parent.tagName &&
                parent.tagName.toLowerCase() === 'a' &&
                parent.href) {
              // This regex identifies strings like void(0),
              // void(0)  ;void(0);, ;;;;
              // which result in a NOP when executed as JavaScript.
              var regex = RegExp("^javascript:(?:(?:void\\(0\\)|;)\\s*)+$");
              if (parent.href.match(regex)) {
                parent = parent.parentNode;
                continue;
              }
              result.href = parent.href;
              result.referrerPolicy = getReferrerPolicy_(parent);
              break;
            }
            parent = parent.parentNode;
          }
          return __gCrWeb.common.JSONStringify(result);
        }
        element = element.parentNode;
      }
    }
    return '{}';
  };

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
      if (evt.keyCode)
        msg.keyCode = evt.keyCode;
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

  function invokeOnHostImmediate_(command) {
    __gCrWeb.message.invokeOnHostImmediate(command);
  };

  /**
   * Gets the referrer policy to use for navigations away from the current page.
   * If a link element is passed, and it includes a rel=noreferrer tag, that
   * will override the page setting.
   * @param {HTMLElement=} opt_linkElement The link triggering the navigation.
   * @return {string} The policy string.
   * @private
   */
  var getReferrerPolicy_ = function(opt_linkElement) {
    if (opt_linkElement) {
      var rel = opt_linkElement.getAttribute('rel');
      if (rel && rel.toLowerCase() == 'noreferrer') {
        return 'never';
      }
    }

    var metaTags = document.getElementsByTagName('meta');
    for (var i = 0; i < metaTags.length; ++i) {
      if (metaTags[i].name.toLowerCase() == 'referrer') {
        return metaTags[i].content.toLowerCase();
      }
    }
    return 'default';
  };

  // Provides a way for other injected javascript to access the page's referrer
  // policy.
  __gCrWeb['getPageReferrerPolicy'] = function() {
    return getReferrerPolicy_();
  };

  // Various aspects of global DOM behavior are overridden here.

  // A popstate event needs to be fired anytime the active history entry
  // changes. Either via back, forward, go navigation or by loading the URL,
  // clicking on a link, etc.
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
    invokeOnHost_({'command': 'window.history.go', 'value': delta});
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

  // Intercept window.close calls.
  window.close = function() {
    invokeOnHost_({'command': 'window.close.self'});
  };

  window.addEventListener('hashchange', function(evt) {
    invokeOnHost_({'command': 'window.hashchange'});
  });

  __gCrWeb.core_dynamic.addEventListeners();

  // Returns if a frame with |name| is found in |currentWindow|.
  // Note frame.name is undefined for cross domain frames.
  var hasFrame_ = function(currentWindow, name) {
    if (currentWindow.name === name)
      return true;

    var frames = currentWindow.frames;
    for (var index = 0; index < frames.length; ++index) {
      var frame = frames[index];
      if (frame === undefined)
        continue;
      if (hasFrame_(frame, name))
        return true;
    }
    return false;
  };

  // Checks if |node| is an anchor to be opened in the current tab.
  var isInternaLink_ = function(node) {
    if (!(node instanceof HTMLAnchorElement))
      return false;

    // Anchor with href='javascript://.....' will be opened in the current tab
    // for simplicity.
    if (node.href.indexOf('javascript:') == 0)
      return true;

    // UIWebView will take care of the following cases.
    //
    // - If the given browsing context name is the empty string or '_self', then
    //   the chosen browsing context must be the current one.
    //
    // - If the given browsing context name is '_parent', then the chosen
    //   browsing context must be the parent browsing context of the current
    //   one, unless there is no one, in which case the chosen browsing context
    //   must be the current browsing context.
    //
    // - If the given browsing context name is '_top', then the chosen browsing
    //   context must be the top-level browsing context of the current one, if
    //   there is one, or else the current browsing context.
    //
    // Here an undefined target is considered in the same way as an empty
    // target.
    if (node.target === undefined || node.target === '' ||
        node.target === '_self' || node.target === '_parent' ||
        node.target === '_top') {
      return true;
    }

    // A new browsing context is being requested for an '_blank' target.
    if (node.target === '_blank')
      return false;

    // Otherwise UIWebView will take care of the case where there exists a
    // browsing context whose name is the same as the given browsing context
    // name. If there is no such a browsing context, a new browsing context is
    // being requested.
    return hasFrame_(window, node.target);
  };

  var getTargetLink_ = function(target) {
    var node = target;
    // Find the closest ancester that is a link.
    while (node) {
      if (node instanceof HTMLAnchorElement)
        break;
      node = node.parentNode;
    }
    return node;
  };

  var setExternalRequest_ = function(href, target) {
    if (typeof(target) == 'undefined' || target == '_blank' || target == '') {
      target = '' + Date.now() + '-' + Math.random();
    }
    if (typeof(href) == 'undefined') {
      // W3C recommended behavior.
      href = 'about:blank';
    }
    // ExternalRequest messages need to be handled before the expected
    // shouldStartLoadWithRequest, as such we cannot wait for the regular
    // message queue invoke which delays to avoid illegal recursion into
    // UIWebView. This immediate class of messages is handled ASAP by
    // CRWWebController.
    invokeOnHostImmediate_({'command': 'externalRequest',
                               'href': href,
                             'target': target,
                     'referrerPolicy': getReferrerPolicy_()});
  };

  var resetExternalRequest_ = function() {
    invokeOnHost_({'command': 'resetExternalRequest'});
  };

  var clickBubbleListener_ = function(evt) {
    if (evt['defaultPrevented']) {
      resetExternalRequest_();
    }
    // Remove the listener.
    evt.currentTarget.removeEventListener(
        'click', clickBubbleListener_, false);
  };

  var getComputedWebkitTouchCallout_ = function(element) {
    return window.getComputedStyle(element, null)['webkitTouchCallout'];
  };

  /**
   * This method applies the various document-level overrides. Sometimes the
   * document object gets reset in the early stages of the page lifecycle, so
   * this is exposed as a method for the application to invoke later. That way
   * the window-level overrides can be applied as soon as possible.
   */
  __gCrWeb.core.documentInject = function() {
    // Perform web view specific operations requiring document.body presence.
    // If necessary returns and waits for document to be present.
    if (!__gCrWeb.core_dynamic.documentInject())
      return;

    document.addEventListener('click', function(evt) {
      var node = getTargetLink_(evt.target);

      if (!node)
        return;

      if (isInternaLink_(node)) {
        return;
      }
      setExternalRequest_(node.href, node.target);
      // Add listener to the target and its immediate ancesters. These event
      // listeners will be removed if they get called. The listeners for some
      // elements might never be removed, but if multiple identical event
      // listeners are registered on the same event target with the same
      // parameters the duplicate instances are discarded.
      for (var level = 0; level < 5; ++level) {
        if (node && node != document) {
          node.addEventListener('click', clickBubbleListener_, false);
          node = node.parentNode;
        } else {
          break;
        }
      }
    }, true);

    // Intercept clicks on anchors (links) during bubbling phase so that the
    // browser can handle target type appropriately.
    document.addEventListener('click', function(evt) {
      var node = getTargetLink_(evt.target);

      if (!node)
        return;

      if (isInternaLink_(node)) {
        if (evt['defaultPrevented'])
          return;
        // Internal link. The web view will handle navigation, but register
        // the anchor for UIWebView to start the progress indicator ASAP and
        // notify web controller as soon as possible of impending navigation.
        if (__gCrWeb.core_dynamic.handleInternalClickEvent) {
          __gCrWeb.core_dynamic.handleInternalClickEvent(node);
        }
        return;
      } else {
        // Resets the external request if it has been canceled, otherwise
        // updates the href in case it has been changed.
        if (evt['defaultPrevented'])
          resetExternalRequest_();
        else
          setExternalRequest_(node.href, node.target);
      }
    }, false);

    // Capture form submit actions.
    document.addEventListener('submit', function(evt) {
      if (evt['defaultPrevented'])
        return;

      var form = evt.target;
      var targetsFrame = form.target && hasFrame_(window, form.target);
      // TODO(stuartmorgan): Handle external targets. crbug.com/233543

      var action = form.getAttribute('action');
      // Default action is to re-submit to same page.
      if (!action)
        action = document.location.href;
      invokeOnHost_({
               'command': 'document.submit',
              'formName': __gCrWeb.common.getFormIdentifier(evt.srcElement),
                  'href': __gCrWeb['getFullyQualifiedURL'](action),
          'targetsFrame': targetsFrame
      });
    }, false);

    addFormEventListeners_();

   // Handle or wait for and handle document load completion, if applicable.
   if (__gCrWeb.core_dynamic.handleDocumentLoaded)
     __gCrWeb.core_dynamic.handleDocumentLoaded();

    return true;
  };

  __gCrWeb.core.documentInject();
}());  // End of anonymous object
