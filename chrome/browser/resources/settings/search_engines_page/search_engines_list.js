// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-list' is a component for showing a
 * list of search engines.
 */
Polymer({
  is: 'settings-search-engines-list',

  properties: {
    /** @type {!Array<!SearchEngine>} */
    engines: Array,

    /** Whether column headers should be displayed */
    hideHeaders: Boolean,

    /**
     * The scroll target that this list should use.
     * @type {?HTMLElement}
     */
    scrollTarget: {
      type: Element,
      value: null,  // Required to populate class.
    },

    /** @private {Object}*/
    lastFocused_: Object,
  },

  /**
   * @param {?HTMLElement} scrollTarget
   * @return {string}
   */
  getIronListClass_: function(scrollTarget) {
    return scrollTarget ? '' : 'fixed-height-list';
  },
});
