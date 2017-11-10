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

      /** @private */
      shownExtensionsCount_: {
        type: Number,
        value: 0,
      },

      /** @private */
      shownAppsCount_: {
        type: Number,
        value: 0,
      },
    },

    /**
     * Computes the filter function to be used for determining which items
     * should be shown. A |null| value indicates that everything should be
     * shown.
     * return {?Function}
     * @private
     */
    computeFilter_: function() {
      const formattedFilter = this.filter.trim().toLowerCase();
      return formattedFilter ?
          i => i.name.toLowerCase().includes(formattedFilter) :
          null;
    },

    /** @private */
    shouldShowEmptyItemsMessage_: function() {
      return !this.isGuest && this.apps.length === 0 &&
          this.extensions.length === 0;
    },

    /** @private */
    shouldShowEmptySearchMessage_: function() {
      return !this.isGuest && !this.shouldShowEmptyItemsMessage_() &&
          this.shownAppsCount_ === 0 && this.shownExtensionsCount_ === 0;
    },
  });

  return {
    ItemList: ItemList,
  };
});
