// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  const ItemList = Polymer({
    is: 'extensions-item-list',

    behaviors: [CrContainerShadowBehavior],

    properties: {
      /** @type {Array<!chrome.developerPrivate.ExtensionInfo>} */
      items: Array,

      /** @type {extensions.ItemDelegate} */
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      isGuest: Boolean,

      filter: String,

      /** @private {Array<!chrome.developerPrivate.ExtensionInfo>} */
      shownItems_: {
        type: Array,
        computed: 'computeShownItems_(items.*, filter)',
      }
    },

    /**
     * Computes the list of items to be shown.
     * @param {Object} changeRecord The changeRecord for |items|.
     * @param {string} filter The updated filter string.
     * @return {Array<!chrome.developerPrivate.ExtensionInfo>}
     * @private
     */
    computeShownItems_: function(changeRecord, filter) {
      const formattedFilter = this.filter.trim().toLowerCase();
      return this.items.filter(
          item => item.name.toLowerCase().includes(formattedFilter));
    },

    /** @private */
    shouldShowEmptyItemsMessage_: function() {
      return !this.isGuest && this.items.length === 0;
    },

    /** @private */
    shouldShowEmptySearchMessage_: function() {
      return !this.isGuest && !this.shouldShowEmptyItemsMessage_() &&
          this.shownItems_.length === 0;
    },
  });

  return {
    ItemList: ItemList,
  };
});
