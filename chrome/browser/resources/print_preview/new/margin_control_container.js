// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-margin-control-container',

  behaviors: [SettingsBehavior],

  properties: {
    /** @type {!print_preview.Size} */
    pageSize: {
      type: Object,
      notify: true,
    },

    previewLoaded: Boolean,

    /** @private {number} */
    scaleTransform_: {
      type: Number,
      notify: true,
      value: 0,
    },

    /** @private {!print_preview.Coordinate2d} */
    translateTransform_: {
      type: Object,
      notify: true,
      value: new print_preview.Coordinate2d(0, 0),
    },

    /** @private {?print_preview.Size} */
    clipSize_: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {boolean} */
    hidden_: {
      type: Boolean,
      notify: true,
      computed: 'computeHidden_(' +
          'previewLoaded, settings.margins.value, pageSize, clipSize_)',
    },

    /**
     * @private {!Array<!print_preview.ticket_items.CustomMarginsOrientation>}
     */
    marginSides_: {
      type: Array,
      notify: true,
      value: [
        print_preview.ticket_items.CustomMarginsOrientation.TOP,
        print_preview.ticket_items.CustomMarginsOrientation.RIGHT,
        print_preview.ticket_items.CustomMarginsOrientation.BOTTOM,
        print_preview.ticket_items.CustomMarginsOrientation.LEFT,
      ],
    },
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHidden_: function() {
    return !this.previewLoaded || !this.clipSize_ ||
        this.getSetting('margins').value !=
        print_preview.ticket_items.MarginsTypeValue.CUSTOM ||
        !this.pageSize;
  },

  /**
   * Updates the translation transformation that translates pixel values in
   * the space of the HTML DOM.
   * @param {print_preview.Coordinate2d} translateTransform Updated value of
   *     the translation transformation.
   */
  updateTranslationTransform: function(translateTransform) {
    if (!translateTransform.equals(this.translateTransform_)) {
      this.translateTransform_ = translateTransform;
    }
  },

  /**
   * Updates the scaling transform that scales pixels values to point values.
   * @param {number} scaleTransform Updated value of the scale transform.
   */
  updateScaleTransform: function(scaleTransform) {
    if (scaleTransform != this.scaleTransform_)
      this.scaleTransform_ = scaleTransform;
  },

  /**
   * Clips margin controls to the given clip size in pixels.
   * @param {print_preview.Size} clipSize Size to clip the margin controls to.
   */
  updateClippingMask: function(clipSize) {
    if (!clipSize) {
      return;
    }
    this.clipSize_ = clipSize;
  },
});
