// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {
  var localStrings = new LocalStrings();

  /**
   * Options for how web history should be handled.
   */
  var HISTORY_STATE_OPTION = {
    PUSH: 1,    // Push a new history state.
    REPLACE: 2, // Replace the current history state.
    NONE: 3,    // Ignore this history state change.
  };

  /**
   * We cache a reference to the #navigation frame here so we don't need to grab
   * it from the DOM on each scroll.
   * @type {Node}
   * @private
   */
  var navFrame;

  /**
   * Handles page initialization.
   */
  function onLoad(e) {
    navFrame = $('navigation');
    navFrame.dataset.width = navFrame.offsetWidth;

    // Select a page based on the page-URL.
    var params = resolvePageInfo();
    showPage(params.id, HISTORY_STATE_OPTION.NONE, params.path);

    window.addEventListener('message', handleWindowMessage);
    window.setTimeout(function() {
      document.documentElement.classList.remove('loading');
    }, 0);

    // HACK(dbeam): This makes the assumption that any second part to a path
    // will result in needing background navigation. We shortcut it to avoid
    // flicker on load.
    // HACK(csilv): Search URLs aren't overlays, special case them.
    if (params.id == 'settings' && params.path &&
        params.path.indexOf('search') != 0) {
      backgroundNavigation();
    }
  }

  /**
   * Find page information from window.location. If the location doesn't
   * point to one of our pages, return default parameters.
   * @return {Object} An object containing the following parameters:
   *     id - The 'id' of the page.
   *     path - A path into the page, including search and hash. Optional.
   */
  function resolvePageInfo() {
    var params = {};
    var path = window.location.pathname;
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

      var container = $(params.id);
      if (container) {
        // The id is valid. Add the hash and search parts of the URL to path.
        params.path = (params.path || '') + window.location.search +
            window.location.hash;
      } else {
        // The target sub-page does not exist, discard the params we generated.
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
      showPage(e.state.pageId, HISTORY_STATE_OPTION.NONE);
  }

  /**
   * @return {Object} The default iframe container.
   */
  function getDefaultIframe() {
    return $(localStrings.getString('helpHost'));
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
    else if (e.data.method === 'setPath')
      setPath(e.origin, e.data.params.path);
    else if (e.data.method === 'setTitle')
      setTitle(e.origin, e.data.params.title);
    else if (e.data.method === 'showPage')
      showPage(e.data.params.pageId, HISTORY_STATE_OPTION.PUSH);
    else if (e.data.method === 'navigationControlsLoaded')
      onNavigationControlsLoaded();
    else if (e.data.method === 'adjustToScroll')
      adjustToScroll(e.data.params);
    else if (e.data.method === 'mouseWheel')
      forwardMouseWheel(e.data.params);
    else
      console.error('Received unexpected message', e.data);
  }

  /**
   * Sends the navigation iframe to the background.
   */
  function backgroundNavigation() {
    navFrame.classList.add('background');
    navFrame.firstChild.tabIndex = -1;
    navFrame.firstChild.setAttribute('aria-hidden', true);
  }

  /**
   * Retrieves the navigation iframe from the background.
   */
  function foregroundNavigation() {
    navFrame.classList.remove('background');
    navFrame.firstChild.tabIndex = 0;
    navFrame.firstChild.removeAttribute('aria-hidden');
  }

  /**
   * Enables or disables animated transitions when changing content while
   * horizontally scrolled.
   * @param {boolean} enabled True if enabled, else false to disable.
   */
  function setContentChanging(enabled) {
    navFrame.classList[enabled ? 'add' : 'remove']('changing-content');

    if (isRTL()) {
      uber.invokeMethodOnWindow(navFrame.firstChild.contentWindow,
                                'setContentChanging',
                                enabled);
    }
  }

  /**
   * Get an iframe based on the origin of a received post message.
   * @param {string} origin The origin of a post message.
   * @return {!HTMLElement} The frame associated to |origin| or null.
   */
  function getIframeFromOrigin(origin) {
    assert(origin.substr(-1) != '/', 'invalid origin given');
    var query = '.iframe-container > iframe[src^="' + origin + '/"]';
    return document.querySelector(query);
  }

  /**
   * Changes the path past the page title (i.e. chrome://chrome/settings/(.*)).
   * @param {string} path The new /path/ to be set after the page name.
   * @param {number} historyOption The type of history modification to make.
   */
  function changePathTo(path, historyOption) {
    assert(!path || path.substr(-1) != '/', 'invalid path given');

    var histFunc;
    if (historyOption == HISTORY_STATE_OPTION.PUSH)
      histFunc = window.history.pushState;
    else if (historyOption == HISTORY_STATE_OPTION.REPLACE)
      histFunc = window.history.replaceState;

    assert(histFunc, 'invalid historyOption given ' + historyOption);

    var pageId = getSelectedIframe().id;
    var args = [{pageId: pageId}, '', '/' + pageId + '/' + (path || '')];
    histFunc.apply(window.history, args);
  }

  /**
   * Sets the "path" of the page (actually the path after the first '/' char).
   * @param {Object} origin The origin of the source iframe.
   * @param {string} title The new "path".
   */
  function setPath(origin, path) {
    assert(!path || path[0] != '/', 'invalid path sent from ' + origin);
    // Only update the currently displayed path if this is the visible frame.
    if (getIframeFromOrigin(origin).parentNode == getSelectedIframe())
      changePathTo(path, HISTORY_STATE_OPTION.REPLACE);
  }

  /**
   * Sets the title of the page.
   * @param {Object} origin The origin of the source iframe.
   * @param {string} title The title of the page.
   */
  function setTitle(origin, title) {
    // Cache the title for the client iframe, i.e., the iframe setting the
    // title. querySelector returns the actual iframe element, so use parentNode
    // to get back to the container.
    var container = getIframeFromOrigin(origin).parentNode;
    container.dataset.title = title;

    // Only update the currently displayed title if this is the visible frame.
    if (container == getSelectedIframe())
      document.title = title;
  }

  /**
   * Selects a subpage. This is called from uber-frame.
   * @param {String} pageId Should matche an id of one of the iframe containers.
   * @param {integer} historyOption Indicates whether we should push or replace
   *     browser history.
   * @param {String} path A sub-page path.
   */
  function showPage(pageId, historyOption, path) {
    var container = $(pageId);
    var lastSelected = document.querySelector('.iframe-container.selected');

    // Lazy load of iframe contents.
    var sourceUrl = container.dataset.url + (path || '');
    var frame = container.querySelector('iframe');
    if (!frame) {
      frame = container.ownerDocument.createElement('iframe');
      container.appendChild(frame);
      frame.src = sourceUrl;
    } else {
      // There's no particularly good way to know what the current URL of the
      // content frame is as we don't have access to its contentWindow's
      // location, so just replace every time until necessary to do otherwise.
      frame.contentWindow.location.replace(sourceUrl);
    }

    // If the last selected container is already showing, ignore the rest.
    if (lastSelected === container)
      return;

    if (lastSelected)
      lastSelected.classList.remove('selected');
    container.classList.add('selected');

    setContentChanging(true);
    adjustToScroll(0);

    var selectedFrame = getSelectedIframe().querySelector('iframe');
    uber.invokeMethodOnWindow(selectedFrame.contentWindow, 'frameSelected');

    if (historyOption != HISTORY_STATE_OPTION.NONE)
      changePathTo(path, historyOption);

    if (container.dataset.title)
      document.title = container.dataset.title;
    $('favicon').href = container.dataset.favicon;

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
      setContentChanging(false);

    if (isRTL()) {
      uber.invokeMethodOnWindow(navFrame.firstChild.contentWindow,
                                'adjustToScroll',
                                scrollOffset);
      var navWidth = Math.max(0, +navFrame.dataset.width + scrollOffset);
      navFrame.style.width = navWidth + 'px';
    } else {
      navFrame.style.webkitTransform = 'translateX(' + -scrollOffset + 'px)';
    }
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
