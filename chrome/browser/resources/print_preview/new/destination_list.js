// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

// TODO (rbpotter): Adjust the short list size based on the dialog height
// instead of always using this constant.
/** @type {number} */
const SHORT_DESTINATION_LIST_SIZE = 4;

Polymer({
  is: 'print-preview-destination-list',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {Array<!print_preview.Destination>} */
    destinations: Array,

    /** @type {boolean} */
    hasActionLink: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    loadingDestinations: {
      type: Boolean,
      value: false,
    },

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @type {boolean} */
    title: String,

    /** @private {boolean} */
    showAll_: {
      type: Boolean,
      value: false,
    },

    /** @private {number} */
    matchingDestinationsCount_: {
      type: Number,
      computed: 'computeMatchingDestinationsCount_(destinations, searchQuery)',
    },

    /** @type {boolean} */
    footerHidden_: {
      type: Boolean,
      computed: 'computeFooterHidden_(matchingDestinationsCount_, showAll_)',
    },

    /** @private {boolean} */
    hasDestinations_: {
      type: Boolean,
      computed: 'computeHasDestinations_(matchingDestinationsCount_)',
    },
  },

  listeners: {
    'dom-change': 'updateListItems_',
  },

  /** @private {number} */
  shownCount_: 0,

  /** @private */
  updateListItems_: function() {
    this.shadowRoot
        .querySelectorAll('print-preview-destination-list-item:not([hidden])')
        .forEach(item => item.update(this.searchQuery));
  },

  /**
   * @return {Function}
   * @private
   */
  computeFilter_: function() {
    this.shownCount_ = 0;
    return destination => {
      const isShown =
          (!this.searchQuery || destination.matches(this.searchQuery)) &&
          (this.shownCount_ < SHORT_DESTINATION_LIST_SIZE || this.showAll_);
      if (isShown)
        this.shownCount_++;
      return isShown;
    };
  },

  /**
   * @return {number}
   * @private
   */
  computeMatchingDestinationsCount_: function() {
    return this.destinations
        .filter(destination => {
          return !this.searchQuery || destination.matches(this.searchQuery);
        })
        .length;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeFooterHidden_: function() {
    return this.matchingDestinationsCount_ < SHORT_DESTINATION_LIST_SIZE ||
        this.showAll_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasDestinations_: function() {
    return this.matchingDestinationsCount_ > 0;
  },

  /** @private */
  onActionLinkTap_: function() {
    print_preview.NativeLayer.getInstance().managePrinters();
  },

  /** @private */
  onShowAllTap_: function() {
    this.showAll_ = true;
  },

  /**
   * @param {!Event} e Event containing the destination that was selected.
   * @private
   */
  onDestinationSelected_: function(e) {
    this.fire(
        'destination-selected', /** @type {!{model: Object}} */ (e).model.item);
  },

  reset: function() {
    this.showAll_ = false;
  },
});
})();
