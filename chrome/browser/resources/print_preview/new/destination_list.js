// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

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

    /** @private {!Array<!print_preview.Destination>} */
    displayedDestinations_: {
      type: Array,
      computed: 'computeDisplayedDestinations_(destinations, searchQuery)',
    },

    /** @type {boolean} */
    footerHidden_: {
      type: Boolean,
      computed: 'computeFooterHidden_(displayedDestinations_, showAll_)',
    },

    /** @private {boolean} */
    hasDestinations_: {
      type: Boolean,
      computed: 'computeHasDestinations_(displayedDestinations_)',
    },
  },

  /**
   * @return {!Array<!print_preview.Destination>}
   * @private
   */
  computeDisplayedDestinations_: function() {
    if (!this.searchQuery)
      return assert(this.destinations);
    return this.destinations.filter(destination => {
      return destination.matches(assert(this.searchQuery));
    });
  },

  /**
   * @param {!print_preview.Destination} destination The destination to get the
   *     search hint for.
   * @return {string} The property or properties matching the search query.
   * @private
   */
  getSearchHint_: function(destination) {
    if (!this.searchQuery)
      return '';
    let hint = '';
    destination.extraPropertiesToMatch.some(property => {
      if (property.match(this.searchQuery))
        hint += property;
    });
    return hint;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeFooterHidden_: function() {
    return this.displayedDestinations_.length < SHORT_DESTINATION_LIST_SIZE ||
        this.showAll_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasDestinations_: function() {
    return this.displayedDestinations_.length > 0;
  },

  /**
   * @param {number} index The index of the destination in the list.
   * @return {boolean}
   * @private
   */
  isDestinationHidden_: function(index) {
    return index >= SHORT_DESTINATION_LIST_SIZE && !this.showAll_;
  },

  /**
   * @param {boolean} offlineOrInvalid Whether the destination is offline or
   *     invalid
   * @return {string} An empty string or 'stale'.
   * @private
   */
  getStaleCssClass_: function(offlineOrInvalid) {
    return offlineOrInvalid ? 'stale' : '';
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
