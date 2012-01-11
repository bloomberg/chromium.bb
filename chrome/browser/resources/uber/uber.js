// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {

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
    var iframe = navItem.associatedIframe;
    var currentIframe =
        document.querySelector('.iframe-container.selected');
    if (currentIframe == iframe)
      return false;

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

  function handleWindowMessage(e) {
    if (e.data === 'showOverlay')
      document.querySelector('.overlay').classList.add('showing');
    else if (e.data === 'hideOverlay')
      document.querySelector('.overlay').classList.remove('showing');
    else
      console.error('Received unexpected message: ' + e.data);
  }

  return {
    onLoad: onLoad,
    onPopHistoryState: onPopHistoryState
  };
});

window.addEventListener('popstate', uber.onPopHistoryState);
document.addEventListener('DOMContentLoaded', uber.onLoad);
