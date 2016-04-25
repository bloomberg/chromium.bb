// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'user-manager-pages' is the element that controls paging in the
 * user manager screen.
 */
Polymer({
  is: 'user-manager-pages',

  properties: {
    /**
     * ID of the currently selected page.
     * @private {string}
     */
    selectedPage_: {
      type: String,
      value: 'user-pods-page'
    },

    /**
     * Data passed to the currently selected page.
     * @private {?Object}
     */
    pageData_: {
      type: Object,
      value: null
    }
  },

  listeners: {
    'change-page': 'changePage_'
  },

  /**
   * Changes the currently selected page.
   * @param {Event} e The event containing ID of the selected page.
   * @private
   */
  changePage_: function(e) {
    this.pageData_ = e.detail.data || null;
    this.selectedPage_ = e.detail.page;
  },

  /**
   * Returns True if the given page should be visible.
   * @param {string} selectedPage ID of the currently selected page.
   * @param {string} page ID of the given page.
   * @return {boolean}
   */
  isPageVisible_: function(selectedPage, page) {
    return this.selectedPage_ == page;
  }
});
