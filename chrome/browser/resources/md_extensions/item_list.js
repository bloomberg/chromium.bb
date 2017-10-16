// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  const ItemList = Polymer({
    is: 'extensions-item-list',

    behaviors: [CrContainerShadowBehavior],

    properties: {
      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      apps: Array,

      /** @type {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      extensions: Array,

      /** @type {extensions.ItemDelegate} */
      delegate: Object,

      inDevMode: {
        type: Boolean,
        value: false,
      },

      isGuest: Boolean,

      filter: String,

      /** @private {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      shownApps_: {
        type: Array,
        computed: 'computeShownItems_(apps, apps.*, filter)',
      },

      /** @private {!Array<!chrome.developerPrivate.ExtensionInfo>} */
      shownExtensions_: {
        type: Array,
        computed: 'computeShownItems_(extensions, extensions.*, filter)',
      }
    },

    /**
     * Computes the list of items to be shown.
     * @param {!Array<!chrome.developerPrivate.ExtensionInfo>} items
     * @return {!Array<!chrome.developerPrivate.ExtensionInfo>}
     * @private
     */
    computeShownItems_: function(items) {
      const formattedFilter = this.filter.trim().toLowerCase();
      return items.filter(
          item => item.name.toLowerCase().includes(formattedFilter));
    },

    /** @private */
    shouldShowEmptyItemsMessage_: function() {
      return !this.isGuest && this.apps.length === 0 &&
          this.extensions.length === 0;
    },

    /** @private */
    shouldShowEmptySearchMessage_: function() {
      return !this.isGuest && !this.shouldShowEmptyItemsMessage_() &&
          this.shownApps_.length === 0 && this.shownExtensions_.length === 0;
    },
  });

  return {
    ItemList: ItemList,
  };
});
