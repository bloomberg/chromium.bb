// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of utility methods for UberPage and its contained
 *     pages.
 */

cr.define('uber', function() {

  /**
   * Fixed position header elements on the page to be shifted by handleScroll.
   * @type {NodeList}
   */
  var headerElements;

  /**
   * This should be called by uber content pages when DOM content has loaded.
   */
  function onContentFrameLoaded() {
    headerElements = document.getElementsByTagName('header');
    document.addEventListener('scroll', handleScroll);

    // Trigger the scroll handler to tell the navigation if our page started
    // with some scroll (happens when you use tab restore).
    handleScroll();

    window.addEventListener('message', handleWindowMessage);
  }

  /**
   * Handles scroll events on the document. This adjusts the position of all
   * headers and updates the parent frame when the page is scrolled.
   * @private
   */
  function handleScroll() {
    var offset = document.body.scrollLeft * -1;
    for (var i = 0; i < headerElements.length; i++) {
      // As a workaround for http://crbug.com/231830, set the transform to
      // 'none' rather than 0px.
      headerElements[i].style.webkitTransform = offset ?
          'translateX(' + offset + 'px)' : 'none';
    }

    invokeMethodOnParent('adjustToScroll', document.body.scrollLeft);
  };

  /**
   * Handles 'message' events on window.
   * @param {Event} e The message event.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'frameSelected')
      handleFrameSelected();
    else if (e.data.method === 'mouseWheel')
      handleMouseWheel(e.data.params);
  }

  /**
   * This is called when a user selects this frame via the navigation bar
   * frame (and is triggered via postMessage() from the uber page).
   * @private
   */
  function handleFrameSelected() {
    document.body.scrollLeft = 0;
  }

  /**
   * Called when a user mouse wheels (or trackpad scrolls) over the nav frame.
   * The wheel event is forwarded here and we scroll the body.
   * There's no way to figure out the actual scroll amount for a given delta.
   * It differs for every platform and even initWebKitWheelEvent takes a
   * pixel amount instead of a wheel delta. So we just choose something
   * reasonable and hope no one notices the difference.
   * @param {Object} params A structure that holds wheel deltas in X and Y.
   */
  function handleMouseWheel(params) {
    document.body.scrollTop -= params.deltaY * 49 / 120;
    document.body.scrollLeft -= params.deltaX * 49 / 120;
  }

  /**
   * Invokes a method on the parent window (UberPage). This is a convenience
   * method for API calls into the uber page.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @private
   */
  function invokeMethodOnParent(method, opt_params) {
    if (window.location == window.parent.location)
      return;

    invokeMethodOnWindow(window.parent, method, opt_params, 'chrome://chrome');
  }

  /**
   * Invokes a method on the target window.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @param {string=} opt_url The origin of the target window.
   * @private
   */
  function invokeMethodOnWindow(targetWindow, method, opt_params, opt_url) {
    var data = {method: method, params: opt_params};
    targetWindow.postMessage(data, opt_url ? opt_url : '*');
  }

  return {
    invokeMethodOnParent: invokeMethodOnParent,
    invokeMethodOnWindow: invokeMethodOnWindow,
    onContentFrameLoaded: onContentFrameLoaded,
  };
});
