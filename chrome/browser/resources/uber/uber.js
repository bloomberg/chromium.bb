// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {

  /**
   * Map from |iframe.src| to the title of |iframe|, cached so the contained
   * pages don't have to update the title on each activation.
   * @private
   */
  var titleMap_ = {};

  /**
   * Handles page initialization.
   */
  function onLoad() {
    var navigationItems = document.querySelectorAll('#navigation li');
    var iframes = document.querySelectorAll('.iframe-container');

    for (var i = 0; i < navigationItems.length; ++i) {
      var navItem = navigationItems[i];
      navItem.associatedIframe = iframes[i];
      iframes[i].associatedNavItem = navItem;
      navItem.addEventListener('click', onNavItemClicked);
    }

    // Update the URL if need be.
    if (window.location.pathname === '/') {
      var selectedNavItem = document.querySelector('#navigation li.selected');
      var pageId = selectedNavItem.associatedIframe.id;
      window.history.replaceState({pageId: pageId}, '', '/' + pageId);
    }

    window.addEventListener('message', handleWindowMessage);
  }

  /**
   * Handles clicks on the navigation controls (switches the page and updates
   * the URL).
   * @param {Event} e The click event.
   */
  function onNavItemClicked(e) {
    if (selectPageForNavItem(e.currentTarget)) {
      var pageId = e.currentTarget.associatedIframe.id;
      window.history.pushState({pageId: pageId}, '', '/' + pageId);
    }
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
    var currentIframe = getSelectedIframe_();
    if (currentIframe == iframe)
      return false;

    // Restore the cached title.
    var iframeEl = iframe.querySelector('iframe');
    var title = titleMap_[iframeEl.src];
    if (title)
      document.title = title;

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
      selectPageForNavItem($(e.state.pageId).associatedNavItem);
  }

  /**
   * @return {Object} The currently selected iframe container.
   * @private
   */
  function getSelectedIframe_() {
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
    if (e.data.method === 'showOverlay')
      showOverlay_();
    else if (e.data.method === 'hideOverlay')
      hideOverlay_();
    else if (e.data.method === 'setTitle')
      setTitle_(e.origin, e.data.params);
    else
      console.error('Received unexpected message: ' + e.data);
  }

  /**
   * @private
   */
  function showOverlay_() {
    document.querySelector('.overlay').classList.add('showing');
  }

  /**
   * @private
   */
  function hideOverlay_() {
    document.querySelector('.overlay').classList.remove('showing');
  }

  /**
   * Sets the title of the page.
   * @param {Object} origin The origin of the source iframe.
   * @param {Object} params Must contain a |title| property.
   * @private
   */
  function setTitle_(origin, params) {
    var container = getSelectedIframe_();
    var iframe = container.querySelector('iframe');

    // |iframe.src| always contains a trailing backslash while |origin| does not
    // so add the trailing source for normalization.
    origin += '/';

    // Only update the currently displayed title if this is the visible frame.
    if (iframe.src === origin)
      document.title = params.title;

    // Cache the title for the selected iframe.
    titleMap_[origin] = params.title;
  }

  return {
    onLoad: onLoad,
    onPopHistoryState: onPopHistoryState
  };

});

window.addEventListener('popstate', uber.onPopHistoryState);
document.addEventListener('DOMContentLoaded', uber.onLoad);
