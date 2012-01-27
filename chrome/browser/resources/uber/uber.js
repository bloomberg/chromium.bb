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
   * Handles page initialization.
   */
  function onLoad(e) {
    // If the pathname points to a sub-page, we may need to alter the location
    // of one of the frames.
    var params = resolvePageInfoFromPath(window.location.pathname);
    if (params.path) {
      var iframe = $(params.id).querySelector('iframe');
      iframe.contentWindow.location.replace(iframe.src + params.path);
    }

    // Select a page based on the page-URL.
    showPage(params.id, HISTORY_STATE_OPTION.REPLACE);

    window.addEventListener('message', handleWindowMessage);
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
   * Selects the page for |navItem|, or does nothing if that item has already
   * been selected.
   * @param {Object} navItem The navigation control li.
   * @return {boolean} True if the item became selected, false if it had already
   *     been selected.
   */
  function selectPageForNavItem(navItem) {
    // Note that |iframe| is the containing element of the underlying iframe, as
    // opposed to the iframe element itself.
    var iframe = navItem.associatedIframe;
    var currentIframe = getSelectedIframe();
    if (currentIframe == iframe)
      return false;

    // Restore the cached title.
    if (iframe.title)
      document.title = iframe.title;

    currentIframe.classList.remove('selected');
    iframe.classList.add('selected');

    var currentNavItem = document.querySelector('li.selected');
    currentNavItem.classList.remove('selected');
    navItem.classList.add('selected');

    return true;
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
      setTitle_(e.origin, e.data.params);
    else if (e.data.method === 'showPage')
      showPage(e.data.params.pageId, HISTORY_STATE_OPTION.PUSH);
    else if (e.data.method === 'navigationControlsLoaded')
      onNavigationControlsLoaded();
    else
      console.error('Received unexpected message: ' + e.data);
  }

  /**
   * Sends the navigation iframe to the background.
   */
  function backgroundNavigation() {
    $('navigation').classList.add('background');
  }

  /**
   * Retrieves the navigation iframe from the background.
   */
  function foregroundNavigation() {
    $('navigation').classList.remove('background');
  }

  /**
   * Sets the title of the page.
   * @param {Object} origin The origin of the source iframe.
   * @param {Object} params Must contain a |title| property.
   */
  function setTitle_(origin, params) {
    // |iframe.src| always contains a trailing backslash while |origin| does not
    // so add the trailing source for normalization.
    var query = '.iframe-container > iframe[src="' + origin + '/"]';

    // Cache the title for the client iframe, i.e., the iframe setting the
    // title. querySelector returns the actual iframe element, so use parentNode
    // to get back to the container.
    var container = document.querySelector(query).parentNode;
    container.title = params.title;

    // Only update the currently displayed title if this is the visible frame.
    if (container == getSelectedIframe())
      document.title = params.title;
  }

  /**
   * Selects a subpage. This is called from uber-frame.
   * @param {String} pageId Should matche an id of one of the iframe containers.
   * @param {integer} historyOption Indicates whether we should push or replace
   *     browser history.
   */
  function showPage(pageId, historyOption) {
    var container = $(pageId);
    var lastSelected = document.querySelector('.iframe-container.selected');
    if (lastSelected === container)
      return;

    if (lastSelected)
      lastSelected.classList.remove('selected');
    container.classList.add('selected');
    document.title = container.title;

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
    uber.invokeMethodOnWindow($('navigation').firstChild.contentWindow,
                              'changeSelection', {pageId: iframe.id});
  }

  return {
    onLoad: onLoad,
    onPopHistoryState: onPopHistoryState
  };

});

window.addEventListener('popstate', uber.onPopHistoryState);
document.addEventListener('DOMContentLoaded', uber.onLoad);
