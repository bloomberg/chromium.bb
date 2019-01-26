// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  const ItemList = Polymer({
    is: 'extensions-item-list',

    behaviors: [CrContainerShadowBehavior, I18nBehavior],

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

      filter: {
        type: String,
      },

      /** @private */
      computedFilter_: {
        type: String,
        computed: 'computeFilter_(filter)',
        observer: 'announceSearchResults_',
      },

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
     * @param {string} id
     * @return {?Element}
     */
    getDetailsButton: function(id) {
      const item = this.$$(`#${id}`);
      return item && item.getDetailsButton();
    },

    /**
     * @param {string} id
     * @return {?Element}
     */
    getErrorsButton: function(id) {
      const item = this.$$(`#${id}`);
      return item && item.getErrorsButton();
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
      if (!this.apps || !this.extensions) {
        return;
      }

      return this.apps.length === 0 && this.extensions.length === 0;
    },

    /** @private */
    shouldShowEmptySearchMessage_: function() {
      return !this.shouldShowEmptyItemsMessage_() &&
          this.shownAppsCount_ === 0 && this.shownExtensionsCount_ === 0;
    },

    /** @private */
    onNoExtensionsTap_: function(e) {
      if (e.target.tagName == 'A') {
        chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
      }
    },

    /** @private */
    announceSearchResults_: function() {
      if (this.computedFilter_) {
        Polymer.IronA11yAnnouncer.requestAvailability();
        this.async(() => {  // Async to allow list to update.
          const total = this.shownAppsCount_ + this.shownExtensionsCount_;
          this.fire('iron-announce', {
            text: this.shouldShowEmptySearchMessage_() ?
                this.i18n('noSearchResults') :
                (total == 1 ?
                     this.i18n('searchResultsSingular', this.filter) :
                     this.i18n(
                         'searchResultsPlural', total.toString(), this.filter)),
          });
        });
      }
    },
  });

  return {
    ItemList: ItemList,
  };
});
