// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The global object.
 * @type {!Object}
 * @const
 */
var global = this;

/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Calls chrome.send with a callback and restores the original afterwards.
 * @param {string} name The name of the message to send.
 * @param {!Array} params The parameters to send.
 * @param {string} callbackName The name of the function that the backend calls.
 * @param {!Function} callback The function to call.
 */
function chromeSend(name, params, callbackName, callback) {
  var old = global[callbackName];
  global[callbackName] = function() {
    // restore
    global[callbackName] = old;

    var args = Array.prototype.slice.call(arguments);
    return callback.apply(global, args);
  };
  chrome.send(name, params);
}

/**
 * Generates a CSS url string.
 * @param {string} s The URL to generate the CSS url for.
 * @return {string} The CSS url string.
 */
function url(s) {
  // http://www.w3.org/TR/css3-values/#uris
  // Parentheses, commas, whitespace characters, single quotes (') and double
  // quotes (") appearing in a URI must be escaped with a backslash
  var s2 = s.replace(/(\(|\)|\,|\s|\'|\"|\\)/g, '\\$1');
  // WebKit has a bug when it comes to URLs that end with \
  // https://bugs.webkit.org/show_bug.cgi?id=28885
  if (/\\\\$/.test(s2)) {
    // Add a space to work around the WebKit bug.
    s2 += ' ';
  }
  return 'url("' + s2 + '")';
}

/**
 * Parses query parameters from Location.
 * @param {string} location The URL to generate the CSS url for.
 * @return {object} Dictionary containing name value pairs for URL
 */
function parseQueryParams(location) {
  var params = {};
  var query = unescape(location.search.substring(1));
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');
    params[pair[0]] = pair[1];
  }
  return params;
}

function findAncestorByClass(el, className) {
  return findAncestor(el, function(el) {
    if (el.classList)
      return el.classList.contains(className);
    return null;
  });
}

/**
 * Return the first ancestor for which the {@code predicate} returns true.
 * @param {Node} node The node to check.
 * @param {function(Node) : boolean} predicate The function that tests the
 *     nodes.
 * @return {Node} The found ancestor or null if not found.
 */
function findAncestor(node, predicate) {
  var last = false;
  while (node != null && !(last = predicate(node))) {
    node = node.parentNode;
  }
  return last ? node : null;
}

function swapDomNodes(a, b) {
  var afterA = a.nextSibling;
  if (afterA == b) {
    swapDomNodes(b, a);
    return;
  }
  var aParent = a.parentNode;
  b.parentNode.replaceChild(a, b);
  aParent.insertBefore(b, afterA);
}

/**
 * Disables text selection and dragging, with optional whitelist callbacks.
 * @param {function(Event):boolean=} opt_allowSelectStart Unless this function
 *    is defined and returns true, the onselectionstart event will be
 *    surpressed.
 * @param {function(Event):boolean=} opt_allowDragStart Unless this function
 *    is defined and returns true, the ondragstart event will be surpressed.
 */
function disableTextSelectAndDrag(opt_allowSelectStart, opt_allowDragStart) {
  // Disable text selection.
  document.onselectstart = function(e) {
    if (!(opt_allowSelectStart && opt_allowSelectStart.call(this, e)))
      e.preventDefault();
  };

  // Disable dragging.
  document.ondragstart = function(e) {
    if (!(opt_allowDragStart && opt_allowDragStart.call(this, e)))
      e.preventDefault();
  };
}

/**
 * Call this to stop clicks on <a href="#"> links from scrolling to the top of
 * the page (and possibly showing a # in the link).
 */
function preventDefaultOnPoundLinkClicks() {
  document.addEventListener('click', function(e) {
    var anchor = findAncestor(e.target, function(el) {
      return el.tagName == 'A';
    });
    // Use getAttribute() to prevent URL normalization.
    if (anchor && anchor.getAttribute('href') == '#')
      e.preventDefault();
  });
}

/**
 * Check the directionality of the page.
 * @return {boolean} True if Chrome is running an RTL UI.
 */
function isRTL() {
  return document.documentElement.dir == 'rtl';
}

/**
 * Simple common assertion API
 * @param {*} condition The condition to test.  Note that this may be used to
 *     test whether a value is defined or not, and we don't want to force a
 *     cast to Boolean.
 * @param {string=} opt_message A message to use in any error.
 */
function assert(condition, opt_message) {
  'use strict';
  if (!condition) {
    var msg = 'Assertion failed';
    if (opt_message)
      msg = msg + ': ' + opt_message;
    throw new Error(msg);
  }
}

/**
 * Get an element that's known to exist by its ID. We use this instead of just
 * calling getElementById and not checking the result because this lets us
 * satisfy the JSCompiler type system.
 * @param {string} id The identifier name.
 * @return {!Element} the Element.
 */
function getRequiredElement(id) {
  var element = $(id);
  assert(element, 'Missing required element: ' + id);
  return element;
}

// Handle click on a link. If the link points to a chrome: or file: url, then
// call into the browser to do the navigation.
document.addEventListener('click', function(e) {
  // Allow preventDefault to work.
  if (!e.returnValue)
    return;

  var el = e.target;
  if (el.nodeType == Node.ELEMENT_NODE &&
      el.webkitMatchesSelector('A, A *')) {
    while (el.tagName != 'A') {
      el = el.parentElement;
    }

    if ((el.protocol == 'file:' || el.protocol == 'about:') &&
        (e.button == 0 || e.button == 1)) {
      chrome.send('navigateToUrl', [
        el.href,
        el.target,
        e.button,
        e.altKey,
        e.ctrlKey,
        e.metaKey,
        e.shiftKey
      ]);
      e.preventDefault();
    }
  }
});

/**
 * Creates a new URL which is the old URL with a GET param of key=value.
 * @param {string} url The base URL. There is not sanity checking on the URL so
 *     it must be passed in a proper format.
 * @param {string} key The key of the param.
 * @param {string} value The value of the param.
 * @return {string} The new URL.
 */
function appendParam(url, key, value) {
  var param = encodeURIComponent(key) + '=' + encodeURIComponent(value);

  if (url.indexOf('?') == -1)
    return url + '?' + param;
  return url + '&' + param;
}

/**
 * Creates a new URL for a favicon request.
 * @param {string} url The url for the favicon.
 * @param {number=} opt_size Optional preferred size of the favicon.
 * @param {boolean=} opt_sessionFavicon Optional flag to indicate if
 *     requesting a session favicon.
 * @return {string} Updated URL for the favicon.
 */
function getFaviconUrl(url, opt_size, opt_sessionFavicon) {
  var size = opt_size || 16;
  var type = opt_sessionFavicon ? 'session-favicon' : 'favicon';
  return 'chrome://' + type + '/size/' + size + '@' +
      window.devicePixelRatio + 'x/' + url;
}

/**
 * Creates an element of a specified type with a specified class name.
 * @param {string} type The node type.
 * @param {string} className The class name to use.
 * @return {Element} The created element.
 */
function createElementWithClassName(type, className) {
  var elm = document.createElement(type);
  elm.className = className;
  return elm;
}
