// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-page-header' shows a basic heading with a 'core-icon'.
 *
 * Example:
 *
 *    <cr-settings-page-header page="{{page}}">
 *    </cr-settings-page-header>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-page-header
 */
Polymer('cr-settings-page-header', {
  publish: {
    /**
     * The current stack of pages being viewed. I.e., may contain subpages.
     *
     * @attribute pageStack
     * @type Array<HTMLElement>
     * @default null
     */
    pageStack: null,

    /**
     * The currently selected page.
     *
     * @attribute selectedPage
     * @type HTMLElement
     * @default null
     */
    selectedPage: null,

    computed: {
      currentPage: 'pageStack[pageStack.length - 1]',
      parentPages: 'getParentPages_(pageStack)',
      topPage: 'pageStack[0]'
    }
  },

  ready: function() {
    this.pageStack = [];
  },

  selectedPageChanged: function() {
    if (this.selectedPage.subpage) {
      // NOTE: Must reassign pageStack rather than doing push() so that the
      // computed property (parentPages) will be notified of the update.
      this.pageStack = this.pageStack.concat(this.selectedPage);
    } else {
      this.pageStack = [this.selectedPage];
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
