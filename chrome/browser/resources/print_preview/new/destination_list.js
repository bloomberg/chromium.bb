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
    hasActionLink: Boolean,

    /** @type {boolean} */
    loadingDestinations: Boolean,

    /** @type {boolean} */
    title: String,

    /** @private {boolean} */
    showAll_: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    footerHidden_: {
      type: Boolean,
      computed: 'computeFooterHidden_(destinations, showAll_)',
    },

    /** @private {boolean} */
    hasDestinations_: {
      type: Boolean,
      computed: 'computeHasDestinations_(destinations)',
    },
  },

  /**
   * @return {boolean}
   * @private
   */
  computeFooterHidden_: function() {
    return this.destinations.length < SHORT_DESTINATION_LIST_SIZE ||
        this.showAll_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasDestinations_: function() {
    return this.destinations.length != 0;
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

  /** @private string */
  totalDestinations_: function() {
    return this.i18n('destinationCount', this.destinations.length.toString());
  },

  /** @private */
  onActionLinkTap_: function() {
    print_preview.NativeLayer.getInstance().managePrinters();
  },

  /** @private */
  onShowAllTap_: function() {
    this.showAll_ = true;
  },
});
})();
