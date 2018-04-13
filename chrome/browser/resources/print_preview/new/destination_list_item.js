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
        'destination.displayName, destination.isOfflineOrInvalid, ' +
        'destination.isExtension)',
  ],

  /** @private {boolean} */
  highlighted_: false,

  /** @private */
  onDestinationPropertiesChange_: function() {
    this.title = this.destination.displayName;
    this.stale_ = this.destination.isOfflineOrInvalid;
    if (this.destination.isExtension) {
      const icon = this.$$('.extension-icon');
      icon.style.backgroundImage = '-webkit-image-set(' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/24/1) 1x,' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/48/1) 2x)';
    }
  },

  /** @private */
  onLearnMoreLinkClick_: function() {
    print_preview.NativeLayer.getInstance().forceOpenNewTab(
        loadTimeData.getString('gcpCertificateErrorLearnMoreURL'));
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
