// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JS utilities automatically injected by GTalk PyAuto tests.
 */

/**
 * Key codes to use with KeyboardEvent.
 */
$KEYS = {
  ENTER: 13,
  ESC: 27
};

/**
 * The first Chrome extension view with a URL containing the query.
 */
$VIEW = function(query) {
  var views = chrome.extension.getViews();
  for (var i = 0; i < views.length; i++) {
    var url = views[i].location.href;
    if (url && url.indexOf(query) >= 0) {
      return views[i];
    }
  }
  return null;
};

/**
 * The body element of the given window.
 */
$BODY = function(opt_window) {
  return (opt_window || window).document.body;
};

/**
 * Find the ancestor of the given element with a particular tag name.
 */
$FindByTagName = function(element, tag, index) {
  var tagElements = element.getElementsByTagName(tag);
  if (index < tagElements.length) {
    return tagElements[index];
  }
  return null;
};

/**
 * Find the first ancestor of the given element containing the given text.
 */
$FindByText = function(baseElement, text) {
  var allElements = baseElement.getElementsByTagName('*');
  for (var i = 0; i < allElements.length; i++) {
    var element = allElements[i];
    if (element.innerText && element.innerText.indexOf(text) >= 0) {
      var child = $FindByText(element, text);
      return child != null ? child : element;
    }
  }
  return null;
};

/**
 * Simulate a click on a given element.
 */
$Click = function(element) {
  var mouseEvent = element.ownerDocument.createEvent('MouseEvent');
  mouseEvent.initMouseEvent('click', true, true, window,
      1, 0, 0, 0, 0, false, false, false, false, 0, null);
  element.dispatchEvent(mouseEvent);
  return true;
};

/**
 * Simulate typing text on a given element.
 */
$Type = function(element, text) {
  var keyEvent = element.ownerDocument.createEvent('TextEvent');
  keyEvent.initTextEvent('textInput', true, true, window, text);
  element.dispatchEvent(keyEvent);
  return true;
};

/**
 * Simulate pressing a certain key on a given element.
 */
$Press = function(baseElement, keycode, opt_ctrlKey, opt_shiftKey,
    opt_altKey, opt_metaKey) {
  var sendKeyEvent = function(element, eventType) {
    // Unfortuantely, using the typical KeyboardEvent and initKeyboardEvent
    // fails in Chrome due to a webkit bug:
    // https://bugs.webkit.org/show_bug.cgi?id=16735
    // We employ a workaround of synthesizing a raw 'Event' suggested here:
    // http://code.google.com/p/selenium/issues/detail?id=567
    var keyEvent = element.ownerDocument.createEvent('Events');
    keyEvent.ctrlKey = Boolean(opt_ctrlKey);
    keyEvent.shiftKey = Boolean(opt_shiftKey);
    keyEvent.altKey = Boolean(opt_altKey);
    keyEvent.metaKey = Boolean(opt_metaKey);
    keyEvent.initEvent(eventType, true, true);
    keyEvent.keyCode = keycode;
    element.dispatchEvent(keyEvent);
  }
  sendKeyEvent(baseElement, 'keydown');
  sendKeyEvent(baseElement, 'keypress');
  sendKeyEvent(baseElement, 'keyup');
  return true;
};
