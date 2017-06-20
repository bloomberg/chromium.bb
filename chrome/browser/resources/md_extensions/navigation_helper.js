// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The different pages that can be shown at a time.
 * Note: This must remain in sync with the page ids in manager.html!
 * @enum {string}
 */
var Page = {
  LIST: 'items-list',
  DETAILS: 'details-view',
  SHORTCUTS: 'keyboard-shortcuts',
  ERRORS: 'error-page',
};

/** @enum {string} */
var Dialog = {
  OPTIONS: 'options',
};

/** @typedef {{page: Page,
               extensionId: (string|undefined),
               subpage: (!Dialog|undefined)}} */
var PageState;

cr.define('extensions', function() {
  'use strict';

  /**
   * A helper object to manage in-page navigations. Since the extensions page
   * needs to support different urls for different subpages (like the details
   * page), we use this object to manage the history and url conversions.
   * @param {!function(!PageState):void} onHistoryChange A function to call when
   *     the page has changed as a result of the user going back or forward in
   *     history; called with the new active page.
   * @constructor */
  function NavigationHelper(onHistoryChange) {
    this.onHistoryChange_ = onHistoryChange;
    window.addEventListener('popstate', this.onPopState_.bind(this));
  }

  NavigationHelper.prototype = {
    /** @private */
    onPopState_: function() {
      this.onHistoryChange_(this.getCurrentPage());
    },

    /**
     * Returns the page that should be displayed for the current URL.
     * @return {!PageState}
     */
    getCurrentPage: function() {
      var search = new URLSearchParams(location.search);
      var id = search.get('id');
      if (id)
        return {page: Page.DETAILS, extensionId: id};
      id = search.get('options');
      if (id)
        return {page: Page.DETAILS, extensionId: id, subpage: Dialog.OPTIONS};
      id = search.get('errors');
      if (id)
        return {page: Page.ERRORS, extensionId: id};

      if (location.pathname == '/shortcuts')
        return {page: Page.SHORTCUTS};

      return {page: Page.LIST};
    },

    /**
     * Called when a page changes, and pushes state to history to reflect it.
     * @param {!PageState} entry
     */
    updateHistory: function(entry) {
      var path;
      switch (entry.page) {
        case Page.LIST:
          path = '/';
          break;
        case Page.DETAILS:
          if (entry.subpage) {
            assert(entry.subpage == Dialog.OPTIONS);
            path = '/?options=' + entry.extensionId;
          } else {
            path = '/?id=' + entry.extensionId;
          }
          break;
        case Page.SHORTCUTS:
          path = '/shortcuts';
          break;
        case Page.ERRORS:
          path = '/?errors=' + entry.extensionId;
          break;
      }
      assert(path);
      var state = {url: path};
      var currentPage = this.getCurrentPage();
      var isDialogNavigation = currentPage.page == entry.page &&
          currentPage.extensionId == entry.extensionId;
      // Navigating to a dialog doesn't visually change pages; it just opens
      // a dialog. As such, we replace state rather than pushing a new state
      // on the stack so that hitting the back button doesn't just toggle the
      // dialog.
      if (isDialogNavigation)
        history.replaceState(state, '', path);
      else
        history.pushState(state, '', path);
    },
  };

  return {NavigationHelper: NavigationHelper};
});
