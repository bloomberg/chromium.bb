// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-page-header' shows a basic heading with a 'iron-icon'.
 *
 * Example:
 *
 *    <cr-settings-page-header page="{{page}}">
 *    </cr-settings-page-header>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-page-header
 */
Polymer({
  is: 'cr-settings-page-header',

  properties: {
    /**
     * The current stack of pages being viewed. I.e., may contain subpages.
     * @type {!Array<!HTMLElement>}
     */
    pageStack: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * The currently selected page.
     * @type {?HTMLElement}
     */
    selectedPage: {
      type: Object,
      observer: 'selectedPageChanged_',
    },

    /**
     * The icon of the page at the root of the stack.
     * @private
     */
    topPageIcon_: {
      type: String,
      computed: 'getTopPageIcon_(pageStack)',
    },

    /**
     * The parent pages of the current page.
     * @private {!Array<!HTMLElement>}
     */
    parentPages_: {
      type: Array,
      computed: 'getParentPages_(pageStack)',
    },

    /**
     * The title of the current page.
     * @private
     */
    currentPageTitle_: {
      type: String,
      computed: 'getCurrentPageTitle_(pageStack)',
    },
  },

  /**
   * @param {!Array<!HTMLElement>} pageStack
   * @return {string} The icon for the top page in the stack.
   * @private
   */
  getTopPageIcon_: function(pageStack) {
    if (pageStack.length == 0)
      return '';
    return pageStack[0].icon;
  },

  /**
   * @param {!HTMLElement} page
   * @return {string} A url link to the given page.
   * @private
   */
  urlFor_: function(page) {
    return page.route ? urlFor(page.route) : '';
  },

  /**
   * @param {!Array<!HTMLElement>} pageStack
   * @return {string} The title of the current page.
   * @private
   */
  getCurrentPageTitle_: function(pageStack) {
    if (pageStack.length == 0)
      return '';
    return pageStack[0].pageTitle;
  },

  /** @private */
  selectedPageChanged_: function() {
    if (this.selectedPage && this.selectedPage.subpage) {
      // NOTE: Must reassign pageStack rather than doing push() so that the
      // computed property (parentPages) will be notified of the update.
      this.pageStack = this.pageStack.concat(this.selectedPage);
    } else if (this.selectedPage) {
      this.pageStack = [this.selectedPage];
    } else {
      this.pageStack = [];
    }
  },

  /**
   * Gets the parent pages in the given page stack; i.e. returns all pages but
   * the topmost subpage.
   *
   * @param {Array<HTMLElement>} stack
   * @return {!Array<HTMLElement>}
   * @private
   */
  getParentPages_: function(stack) {
    if (!stack || stack.length <= 1) {
      return [];
    }

    return stack.slice(0, stack.length - 1);
  },
});
