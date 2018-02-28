// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-list-item',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @private */
    stale_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private {string} */
    searchHint_: String,
  },

  observers: [
    'onDestinationPropertiesChange_(' +
        'destination.displayName, destination.isOfflineOrInvalid)',
  ],

  /** @private {boolean} */
  highlighted_: false,

  /** @private */
  onDestinationPropertiesChange_: function() {
    this.title = this.destination.displayName;
    this.stale_ = this.destination.isOfflineOrInvalid;
  },

  update: function() {
    this.updateSearchHint_();
    this.updateHighlighting_();
  },

  /** @private */
  updateSearchHint_: function() {
    this.searchHint_ = !this.searchQuery ?
        '' :
        this.destination.extraPropertiesToMatch
            .filter(p => p.match(this.searchQuery))
            .join(' ');
  },

  /** @private */
  updateHighlighting_: function() {
    this.highlighted_ = print_preview.updateHighlights(
        this, this.searchQuery, this.highlighted_);
  },
});
