// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-select',

  behaviors: [I18nBehavior],

  properties: {
    activeUser: String,

    appKioskMode: Boolean,

    /** @type {!print_preview.CloudPrintState} */
    cloudPrintState: Number,

    /** @type {!print_preview.Destination} */
    destination: Object,

    disabled: Boolean,

    noDestinationsFound: Boolean,

    /** @type {!Array<!print_preview.Destination>} */
    recentDestinationList: Array,

    /** @private {boolean} */
    showGoogleDrive_: {
      type: Boolean,
      computed: 'computeShowGoogleDrive_(' +
          'recentDestinationList.*, cloudPrintState)',
    },

    /** @private {boolean} */
    showSaveAsPdf_: {
      type: Boolean,
      computed: 'computeShowSaveAsPdf_(recentDestinationList.*, appKioskMode)',
    },
  },

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Polymer.Base.create('iron-meta', {type: 'iconset'})),

  /** Sets the select to the current value of |destination|. */
  updateDestination: function() {
    this.$$('.md-select').value = this.destination.key;
  },

  /**
   * @return {boolean} Whether to show the Google Drive option.
   * @private
   */
  computeShowSaveAsPdf_: function() {
    return !this.appKioskMode && this.recentDestinationList &&
        !this.recentDestinationList.some(destination => {
          return destination.id ===
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
        });
  },

  /**
   * @return {boolean} Whether to show the Google Drive option.
   * @private
   */
  computeShowGoogleDrive_: function() {
    return this.cloudPrintState === print_preview.CloudPrintState.SIGNED_IN &&
        this.recentDestinationList &&
        !this.recentDestinationList.some(
            destination => destination.id ===
                print_preview.Destination.GooglePromotedId.DOCS);
  },

  /**
   * @return {string} Unique identifier for the Save as PDF destination
   * @private
   */
  getPdfDestinationKey_: function() {
    return print_preview.createDestinationKey(
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        print_preview.DestinationOrigin.LOCAL, '');
  },

  /**
   * @return {string} Unique identifier for the Save to Google Drive destination
   * @private
   */
  getGoogleDriveDestinationKey_: function() {
    return print_preview.createDestinationKey(
        print_preview.Destination.GooglePromotedId.DOCS,
        print_preview.DestinationOrigin.COOKIES, this.activeUser);
  },

  /**
   * @param {string} icon The icon set and icon to obtain.
   * @return {string} An inline svg corresponding to |icon|.
   * @private
   */
  getIconImage_: function(icon) {
    if (!icon) {
      return '';
    }

    const iconSetAndIcon =
        this.noDestinationsFound ? ['cr', 'error'] : icon.split(':');
    const iconset = /** @type {Polymer.IronIconsetSvg} */ (
        this.meta_.byKey(iconSetAndIcon[0]));
    const serializer = new XMLSerializer();
    const iconElement = iconset.createIcon(iconSetAndIcon[1], isRTL());
    iconElement.style.fill = '#80868B';
    const serializedIcon = serializer.serializeToString(iconElement);
    return 'url("data:image/svg+xml;charset=utf-8,' +
        encodeURIComponent(serializedIcon) + '")';
  },

  /** @private */
  onSelectedDestinationOptionChange_: function() {
    this.fire('selected-option-change', this.$$('.md-select').value);
  },
});
