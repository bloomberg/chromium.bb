// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {

  /**
   * Options for how web history should be handled.
   **/
  var HISTORY_STATE_OPTION = {
    PUSH: 1,    // Push a new history state.
    REPLACE: 2  // Replace the current history state.
  };

  /**
  * We cache a reference to the #navigation frame here to so we don't need to
  * grab it from the DOM on each scroll.
  * @type {Node}
  * @private
  * @static
  */
  var navFrame;

  /**
   * Handles page initialization.
   */
  function onLoad(e) {
    navFrame = $('navigation');

    // Select a page based on the page-URL.
    var params = resolvePageInfoFromPath(window.location.pathname);
    showPage(params.id, HISTORY_STATE_OPTION.REPLACE, params.path);

    window.addEventListener('message', handleWindowMessage);
    window.setTimeout(function() {
      document.documentElement.classList.remove('loading');
    }, 0);
  }

  /**
   * Find page information from the given path. If the path doesn't point to one
   * of our pages, return default parameters.
   * @param {string} path A path taken from the page URL.
   * @return {Object} An object containining the following parameters:
   *     id - The 'id' of the page.
   *     path - A path into the page. Optional.
   */
  function resolvePageInfoFromPath(path) {
    var params = {};
    if (path.length > 1) {
      // Split the path into id and the remaining path.
      path = path.slice(1);
      var index = path.indexOf('/');
      if (index != -1) {
        params.id = path.slice(0, index);
        params.path = path.slice(index + 1);
      } else {
        params.id = path;
      }
      // If the target sub-page does not exist, discard the params we
      // generated.
      var container = $(params.id);
      if (!container) {
        params.id = undefined;
        params.path = undefined;
      }
    }
    // If we don't have a valid page, get a default.
    if (!params.id)
      params.id = getDefaultIframe().id;

    return params;
  }

  /**
   * Handler for window.onpopstate.
   * @param {Event} e The history event.
   */
  function onPopHistoryState(e) {
    if (e.state && e.state.pageId)
      showPage(e.state.pageId, HISTORY_STATE_OPTION.PUSH);
  }

  /**
   * @return {Object} The default iframe container.
   */
  function getDefaultIframe() {
    // TODO(csilv): This will select the first iframe as the default, but
    // perhaps we want to use some other logic?
    return document.querySelector('.iframe-container');
  }

  /**
   * @return {Object} The currently selected iframe container.
   */
  function getSelectedIframe() {
    return document.querySelector('.iframe-container.selected');
  }

  /**
   * Handles postMessage calls from the iframes of the contained pages.
   *
   * The pages request functionality from this object by passing an object of
   * the following form:
   *
   *  { method : "methodToInvoke",
   *    params : {...}
   *  }
   *
   * |method| is required, while |params| is optional. Extra parameters required
   * by a method must be specified by that method's documentation.
   *
   * @param {Event} e The posted object.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'beginInterceptingEvents')
      backgroundNavigation();
    else if (e.data.method === 'stopInterceptingEvents')
      foregroundNavigation();
    else if (e.data.method === 'setTitle')
      setTitle_(e.origin, e.data.params.title);
    else if (e.data.method === 'showPage')
      showPage(e.data.params.pageId, HISTORY_STATE_OPTION.PUSH);
    else if (e.data.method === 'navigationControlsLoaded')
      onNavigationControlsLoaded();
    else if (e.data.method === 'adjustToScroll')
      adjustToScroll(e.data.params);
    else if (e.data.method === 'mouseWheel')
      forwardMouseWheel(e.data.params);
    else
      console.error('Received unexpected message',  e.data);
  }

  /**
   * Sends the navigation iframe to the background.
   */
  function backgroundNavigation() {
    navFrame.classList.add('background');
  }

  /**
   * Retrieves the navigation iframe from the background.
   */
  function foregroundNavigation() {
    navFrame.classList.remove('background');
  }

  /**
   * Enables animated transitions when horizontally scrolling.
   */
  function enableScrollEasing() {
    navFrame.classList.add('animating');
  }

  /**
   * Disables animated transitions when horizontally scrolling.
   */
  function disableScrollEasing() {
    navFrame.classList.remove('animating');
  }

  /**
   * Sets the title of the page.
   * @param {Object} origin The origin of the source iframe.
   * @param {string} title The title of the page.
   */
  function setTitle_(origin, title) {
    // |iframe.src| always contains a trailing backslash while |origin| does not
    // so add the trailing source for normalization.
    var query = '.iframe-container > iframe[src="' + origin + '/"]';

    // Cache the title for the client iframe, i.e., the iframe setting the
    // title. querySelector returns the actual iframe element, so use parentNode
    // to get back to the container.
    var container = document.querySelector(query).parentNode;
    container.title = title;

    // Only update the currently displayed title if this is the visible frame.
    if (container == getSelectedIframe())
      document.title = title;
  }

  /**
   * Selects a subpage. This is called from uber-frame.
   * @param {String} pageId Should matche an id of one of the iframe containers.
   * @param {integer} historyOption Indicates whether we should push or replace
   *     browser history.
   * @param {String=} path An optional sub-page path.
   */
  function showPage(pageId, historyOption, path) {
    var container = $(pageId);
    var lastSelected = document.querySelector('.iframe-container.selected');
    if (lastSelected === container)
      return;

    // Lazy load of iframe contents.
    var frame = container.querySelector('iframe');
    var sourceURL = frame.dataset.url;
    if (!frame.getAttribute('src'))
      frame.src = sourceURL;
    // If there is a non-empty path, alter the location of the frame.
    if (path && path.length)
      frame.contentWindow.location.replace(sourceURL + path);

    if (lastSelected)
      lastSelected.classList.remove('selected');
    container.classList.add('selected');
    document.title = container.title;
    $('favicon').href = container.dataset.favicon;

    enableScrollEasing();
    adjustToScroll(0);

    var selectedFrame = getSelectedIframe().querySelector('iframe');
    uber.invokeMethodOnWindow(selectedFrame.contentWindow, 'frameSelected');

    if (historyOption == HISTORY_STATE_OPTION.PUSH)
      window.history.pushState({pageId: pageId}, '', '/' + pageId);
    else if (historyOption == HISTORY_STATE_OPTION.REPLACE)
      window.history.replaceState({pageId: pageId}, '', '/' + pageId);

    updateNavigationControls();
  }

  function onNavigationControlsLoaded() {
    updateNavigationControls();
  }

  /**
   * Sends a message to uber-frame to update the appearance of the nav controls.
   * It should be called whenever the selected iframe changes.
   */
  function updateNavigationControls() {
    var iframe = getSelectedIframe();
    uber.invokeMethodOnWindow(navFrame.firstChild.contentWindow,
                              'changeSelection', {pageId: iframe.id});
  }

  /**
   * Forwarded scroll offset from a content frame's scroll handler.
   * @param {number} scrollOffset The scroll offset from the content frame.
   */
  function adjustToScroll(scrollOffset) {
    // NOTE: The scroll is reset to 0 and easing turned on every time a user
    // switches frames. If we receive a non-zero value it has to have come from
    // a real user scroll, so we disable easing when this happens.
    if (scrollOffset != 0)
      disableScrollEasing();
    navFrame.style.webkitTransform =
        'translateX(' + (scrollOffset * -1) + 'px)';
  }

  /**
   * Forward scroll wheel events to subpages.
   * @param {Object} params Relevant parameters of wheel event.
   */
  function forwardMouseWheel(params) {
    var iframe = getSelectedIframe().querySelector('iframe');
    uber.invokeMethodOnWindow(iframe.contentWindow, 'mouseWheel', params);
  }

  return {
    onLoad: onLoad,
    onPopHistoryState: onPopHistoryState
  };

});

window.addEventListener('popstate', uber.onPopHistoryState);
document.addEventListener('DOMContentLoaded', uber.onLoad);
