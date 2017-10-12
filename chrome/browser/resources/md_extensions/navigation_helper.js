// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

/**
 * The different pages that can be shown at a time.
 * Note: This must remain in sync with the page ids in manager.html!
 * @enum {string}
 */
const Page = {
  LIST: 'items-list',
  DETAILS: 'details-view',
  SHORTCUTS: 'keyboard-shortcuts',
  ERRORS: 'error-page',
};

/** @enum {string} */
const Dialog = {
  OPTIONS: 'options',
};

/** @enum {number} */
extensions.ShowingType = {
  EXTENSIONS: 0,
  APPS: 1,
};

/** @typedef {{page: Page,
               extensionId: (string|undefined),
               subpage: (!Dialog|undefined),
               type: (!extensions.ShowingType|undefined)}} */
let PageState;

cr.define('extensions', function() {
  'use strict';

  /**
   * Regular expression that captures the leading slash, the content and the
   * trailing slash in three different groups.
   * @const {!RegExp}
   */
  const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

  /**
   * A helper object to manage in-page navigations. Since the extensions page
   * needs to support different urls for different subpages (like the details
   * page), we use this object to manage the history and url conversions.
   */
  class NavigationHelper {
    constructor() {
      // Redirect if route not supported.
      let validPathnames = ['/'];
      if (!loadTimeData.getBoolean('isGuest')) {
        validPathnames.push('/shortcuts', '/apps');
      }
      if (!validPathnames.includes(this.currentPath_)) {
        window.history.replaceState(undefined, '', '/');
      }

      /** @private {number} */
      this.nextListenerId_ = 1;

      /** @private {!Map<number, function(!PageState)>} */
      this.listeners_ = new Map();

      window.addEventListener('popstate', () => {
        this.notifyRouteChanged_(this.getCurrentPage());
      });
    }

    /** @private */
    get currentPath_() {
      return location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    }

    /**
     * @return {!PageState} The page that should be displayed for the current
     *     URL.
     */
    getCurrentPage() {
      const search = new URLSearchParams(location.search);
      let id = search.get('id');
      if (id)
        return {page: Page.DETAILS, extensionId: id};
      id = search.get('options');
      if (id)
        return {page: Page.DETAILS, extensionId: id, subpage: Dialog.OPTIONS};
      id = search.get('errors');
      if (id)
        return {page: Page.ERRORS, extensionId: id};

      if (this.currentPath_ == '/shortcuts')
        return {page: Page.SHORTCUTS};

      if (this.currentPath_ == '/apps')
        return {page: Page.LIST, type: extensions.ShowingType.APPS};

      return {page: Page.LIST, type: extensions.ShowingType.EXTENSIONS};
    }

    /**
     * Function to add subscribers.
     * @param {!function(!PageState)} listener
     * @return {number} A numerical ID to be used for removing the listener.
     */
    addListener(listener) {
      const nextListenerId = this.nextListenerId_++;
      this.listeners_.set(nextListenerId, listener);
      return nextListenerId;
    }

    /**
     * Remove a previously registered listener.
     * @param {number} id
     * @return {boolean} Whether a listener with the given ID was actually found
     *     and removed.
     */
    removeListener(id) {
      return this.listeners_.delete(id);
    }

    /**
     * Function to notify subscribers.
     * @private
     */
    notifyRouteChanged_(newPage) {
      this.listeners_.forEach((listener, id) => {
        listener(newPage);
      });
    }

    /**
     * @param {!PageState} newPage the page to navigate to.
     */
    navigateTo(newPage) {
      let currentPage = this.getCurrentPage();
      if (currentPage && currentPage.page == newPage.page &&
          currentPage.type == newPage.type &&
          currentPage.subpage == newPage.subpage &&
          currentPage.extensionId == newPage.extensionId) {
        return;
      }

      this.updateHistory(newPage);
      this.notifyRouteChanged_(newPage);
    }

    /**
     * Called when a page changes, and pushes state to history to reflect it.
     * @param {!PageState} entry
     */
    updateHistory(entry) {
      let path;
      switch (entry.page) {
        case Page.LIST:
          if (entry.type == extensions.ShowingType.APPS)
            path = '/apps';
          else
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
      const state = {url: path};
      const currentPage = this.getCurrentPage();
      const isDialogNavigation = currentPage.page == entry.page &&
          currentPage.extensionId == entry.extensionId &&
          currentPage.type == entry.type;
      // Navigating to a dialog doesn't visually change pages; it just opens
      // a dialog. As such, we replace state rather than pushing a new state
      // on the stack so that hitting the back button doesn't just toggle the
      // dialog.
      if (isDialogNavigation)
        history.replaceState(state, '', path);
      else
        history.pushState(state, '', path);
    }
  }

  const navigation = new NavigationHelper();

  return {
    // Constructor exposed for testing purposes.
    NavigationHelper: NavigationHelper,
    navigation: navigation,
  };
});
