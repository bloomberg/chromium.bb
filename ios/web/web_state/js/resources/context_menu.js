// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview APIs used by CRWContextMenuController.
 */

goog.provide('__crWeb.contextMenu');

/** Beginning of anonymous object */
(function() {

/**
 * Finds the url of the image or link under the selected point. Sends the
 * found element (or an empty object if no links or images are found) back to
 * the application by posting a 'FindElementResultHandler' message.
 * The object returned in the message is of the same form as
 * {@code getElementFromPointInPageCoordinates} result.
 * @param {string} identifier An identifier which be returned in the result
 *                 dictionary of this request.
 * @param {number} x Horizontal center of the selected point in web view
 *                 coordinates.
 * @param {number} y Vertical center of the selected point in web view
 *                 coordinates.
 * @param {number} webViewWidth the width of web view.
 * @param {number} webViewHeight the height of web view.
 */
__gCrWeb['findElementAtPoint'] =
    function(requestID, x, y, webViewWidth, webViewHeight) {
      var scale = getPageWidth() / webViewWidth;
      var result = getElementFromPointInPageCoordinates(x * scale, y * scale);
      result.requestID = requestID;
      __gCrWeb.common.sendWebKitMessage('FindElementResultHandler', result);
    };

/**
 * Returns the url of the image or link under the selected point. Returns an
 * empty object if no links or images are found.
 * @param {number} x Horizontal center of the selected point in web view
 *                   coordinates.
 * @param {number} y Vertical center of the selected point in web view
 *                 coordinates.
 * @param {number} webViewWidth the width of web view.
 * @param {number} webViewHeight the height of web view.
 * @return {!Object} An object in the same form as
 *                   {@code getElementFromPointInPageCoordinates} result.
 */
__gCrWeb['getElementFromPoint'] = function(x, y, webViewWidth, webViewHeight) {
  var scale = getPageWidth() / webViewWidth;
  return getElementFromPointInPageCoordinates(x * scale, y * scale);
};

/**
 * Suppresses the next click such that they are not handled by JS click
 * event handlers.
 * @type {void}
 */
__gCrWeb['suppressNextClick'] = function() {
  var suppressNextClick = function(evt) {
    evt.preventDefault();
    document.removeEventListener('click', suppressNextClick, false);
  };
  document.addEventListener('click', suppressNextClick);
};

/**
 * Returns an object representing the details of the given element.
 * @param {number} x Horizontal center of the selected point in page
 *                 coordinates.
 * @param {number} y Vertical center of the selected point in page
 *                 coordinates.
 * @return {Object} null if no element was found or an object of the form {
 *     href,  // URL of the link under the point
 *     innerText,  // innerText of the link, if the selected element is a link
 *     src,  // src of the image, if the selected element is an image
 *     title,  // title of the image, if the selected
 *     referrerPolicy
 *   }
 *   where:
 *     <ul>
 *     <li>href, innerText are set if the selected element is a link.
 *     <li>src, title are set if the selected element is an image.
 *     <li>href is also set if the selected element is an image with a link.
 *     <li>referrerPolicy is the referrer policy to use for navigations away
 *         from the current page.
 *     </ul>
 */
var getElementFromPointInPageCoordinates = function(x, y) {
  var hitCoordinates = spiralCoordinates_(x, y);
  for (var index = 0; index < hitCoordinates.length; index++) {
    var coordinates = hitCoordinates[index];

    var coordinateDetails = newCoordinate(coordinates.x, coordinates.y);
    var element = elementsFromCoordinates(coordinateDetails);
    if (!element || !element.tagName) {
      // Nothing under the hit point. Try the next hit point.
      continue;
    }

    // Also check element's ancestors. A bound on the level is used here to
    // avoid large overhead when no links or images are found.
    var level = 0;
    while (++level < 8 && element && element != document) {
      var tagName = element.tagName;
      if (!tagName) continue;
      tagName = tagName.toLowerCase();

      if (tagName === 'input' || tagName === 'textarea' ||
          tagName === 'select' || tagName === 'option') {
        // If the element is a known input element, stop the spiral search and
        // return empty results.
        return {};
      }

      if (getComputedWebkitTouchCallout_(element) !== 'none') {
        if (tagName === 'a' && element.href) {
          // Found a link.
          return {
            href: element.href,
            referrerPolicy: getReferrerPolicy_(element),
            innerText: element.innerText
          };
        }

        if (tagName === 'img' && element.src) {
          // Found an image.
          var result = {src: element.src, referrerPolicy: getReferrerPolicy_()};
          // Copy the title, if any.
          if (element.title) {
            result.title = element.title;
          }
          // Check if the image is also a link.
          var parent = element.parentNode;
          while (parent) {
            if (parent.tagName && parent.tagName.toLowerCase() === 'a' &&
                parent.href) {
              // This regex identifies strings like void(0),
              // void(0)  ;void(0);, ;;;;
              // which result in a NOP when executed as JavaScript.
              var regex = RegExp('^javascript:(?:(?:void\\(0\\)|;)\\s*)+$');
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
          return result;
        }
      }
      element = element.parentNode;
    }
  }
  return {};
};

/**
 * Returns the margin in points around touchable elements (e.g. links for
 * custom context menu).
 * @type {number}
 */
var getPageWidth = function() {
  var documentElement = document.documentElement;
  var documentBody = document.body;
  return Math.max(
      documentElement.clientWidth, documentElement.scrollWidth,
      documentElement.offsetWidth, documentBody.scrollWidth,
      documentBody.offsetWidth);
};

/**
 * Returns whether or not view port coordinates should be used for the given
 * window.
 * @return {boolean} True if the window has been scrolled down or to the right,
 *                   false otherwise.
 */
var elementFromPointIsUsingViewPortCoordinates = function(win) {
  if (win.pageYOffset > 0) {  // Page scrolled down.
    return (
        win.document.elementFromPoint(
            0, win.pageYOffset + win.innerHeight - 1) === null);
  }
  if (win.pageXOffset > 0) {  // Page scrolled to the right.
    return (
        win.document.elementFromPoint(
            win.pageXOffset + win.innerWidth - 1, 0) === null);
  }
  return false;  // No scrolling, don't care.
};

/**
 * Returns the coordinates of the upper left corner of |obj| in the
 * coordinates of the window that |obj| is in.
 * @param {HTMLElement} el The element whose coordinates will be returned.
 * @return {!Object} coordinates of the given object.
 */
var getPositionInWindow = function(el) {
  var coord = {x: 0, y: 0};
  while (el.offsetParent) {
    coord.x += el.offsetLeft;
    coord.y += el.offsetTop;
    el = el.offsetParent;
  }
  return coord;
};

/**
 * Returns details about a given coordinate in {@code window}.
 * @param {number} x The x component of the coordinate in {@code window}.
 * @param {number} y The y component of the coordinate in {@code window}.
 * @return {!Object} Details about the given coordinate and the current window.
 */
var newCoordinate = function(x, y) {
  var coordinates = {
    x: x,
    y: y,
    viewPortX: x - window.pageXOffset,
    viewPortY: y - window.pageYOffset,
    useViewPortCoordinates: false,
    window: window
  };
  return coordinates;
};

/**
 * Returns the element at the given coordinates.
 * @param {Object} coordinates Page coordinates in the same format as the result
 *                             from {@code newCoordinate}.
 */
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
    // Check if the frame is in a different domain using only information
    // visible to the current frame (i.e. currentElement.src) to avoid
    // triggering a SecurityError in the console.
    if (!__gCrWeb.common.isSameOrigin(
        window.location.href, currentElement.src)) {
      return currentElement;
    }
    var framePosition = getPositionInWindow(currentElement);
    coordinates.viewPortX -= framePosition.x - coordinates.window.pageXOffset;
    coordinates.viewPortY -= framePosition.y - coordinates.window.pageYOffset;
    coordinates.window = currentElement.contentWindow;
    coordinates.x -= framePosition.x + coordinates.window.pageXOffset;
    coordinates.y -= framePosition.y + coordinates.window.pageYOffset;
    return elementsFromCoordinates(coordinates);
  }
  return currentElement;
};

/** @private
 * @param {number} x
 * @param {number} y
 * @return {Object}
 */
var spiralCoordinates_ = function(x, y) {
  var MAX_ANGLE = Math.PI * 2.0 * 3.0;
  var POINT_COUNT = 30;
  var ANGLE_STEP = MAX_ANGLE / POINT_COUNT;
  var TOUCH_MARGIN = 25;
  var SPEED = TOUCH_MARGIN / MAX_ANGLE;

  var coordinates = [];
  for (var index = 0; index < POINT_COUNT; index++) {
    var angle = ANGLE_STEP * index;
    var radius = angle * SPEED;

    coordinates.push({
      x: x + Math.round(Math.cos(angle) * radius),
      y: y + Math.round(Math.sin(angle) * radius)
    });
  }

  return coordinates;
};

/** @private
 * @param {HTMLElement} element
 * @return {Object}
 */
var getComputedWebkitTouchCallout_ = function(element) {
  return window.getComputedStyle(element, null)['webkitTouchCallout'];
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

  // Search for referrer meta tag.  WKWebView only supports a subset of values
  // for referrer meta tags.  If it parses a referrer meta tag with an
  // unsupported value, it will default to 'never'.
  var metaTags = document.getElementsByTagName('meta');
  for (var i = 0; i < metaTags.length; ++i) {
    if (metaTags[i].name.toLowerCase() == 'referrer') {
      var referrerPolicy = metaTags[i].content.toLowerCase();
      if (referrerPolicy == 'default' || referrerPolicy == 'always' ||
          referrerPolicy == 'no-referrer' || referrerPolicy == 'origin' ||
          referrerPolicy == 'no-referrer-when-downgrade' ||
          referrerPolicy == 'unsafe-url') {
        return referrerPolicy;
      } else {
        return 'never';
      }
    }
  }
  return 'default';
};

}());  // End of anonymouse object
