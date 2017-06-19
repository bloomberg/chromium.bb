// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview.ticket_items');

/**
 * Enumeration of the orientations of margins.
 * @enum {string}
 */
print_preview.ticket_items.CustomMarginsOrientation = {
  TOP: 'top',
  RIGHT: 'right',
  BOTTOM: 'bottom',
  LEFT: 'left'
};

cr.define('print_preview.ticket_items', function() {
  'use strict';

  var CustomMarginsOrientation =
      print_preview.ticket_items.CustomMarginsOrientation;

  /**
   * Custom page margins ticket item whose value is a
   * {@code print_preview.Margins}.
   * @param {!print_preview.AppState} appState App state used to persist custom
   *     margins.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function CustomMargins(appState, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this, appState, print_preview.AppStateField.CUSTOM_MARGINS,
        null /*destinationStore*/, documentInfo);
  }

  /**
   * Mapping of a margin orientation to its opposite.
   * @type {!Object<!print_preview.ticket_items.CustomMarginsOrientation,
   *                 !print_preview.ticket_items.CustomMarginsOrientation>}
   * @private
   */
  CustomMargins.OppositeOrientation_ = {};
  CustomMargins.OppositeOrientation_[CustomMarginsOrientation.TOP] =
      CustomMarginsOrientation.BOTTOM;
  CustomMargins.OppositeOrientation_[CustomMarginsOrientation.RIGHT] =
      CustomMarginsOrientation.LEFT;
  CustomMargins.OppositeOrientation_[CustomMarginsOrientation.BOTTOM] =
      CustomMarginsOrientation.TOP;
  CustomMargins.OppositeOrientation_[CustomMarginsOrientation.LEFT] =
      CustomMarginsOrientation.RIGHT;

  /**
   * Minimum distance in points that two margins can be separated by.
   * @type {number}
   * @const
   * @private
   */
  CustomMargins.MINIMUM_MARGINS_DISTANCE_ = 72;  // 1 inch.

  CustomMargins.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      var margins = /** @type {!print_preview.Margins} */ (value);
      for (var key in CustomMarginsOrientation) {
        var o = CustomMarginsOrientation[key];
        var max = this.getMarginMax_(
            o, margins.get(CustomMargins.OppositeOrientation_[o]));
        if (margins.get(o) > max || margins.get(o) < 0) {
          return false;
        }
      }
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    isValueEqual: function(value) {
      return this.getValue().equals(value);
    },

    /**
     * @param {!print_preview.ticket_items.CustomMarginsOrientation}
     *     orientation Specifies the margin to get the maximum value for.
     * @return {number} Maximum value in points of the specified margin.
     */
    getMarginMax: function(orientation) {
      var oppositeOrient = CustomMargins.OppositeOrientation_[orientation];
      var margins = /** @type {!print_preview.Margins} */ (this.getValue());
      return this.getMarginMax_(orientation, margins.get(oppositeOrient));
    },

    /** @override */
    updateValue: function(value) {
      var margins = /** @type {!print_preview.Margins} */ (value);
      if (margins != null) {
        margins = new print_preview.Margins(
            Math.round(margins.get(CustomMarginsOrientation.TOP)),
            Math.round(margins.get(CustomMarginsOrientation.RIGHT)),
            Math.round(margins.get(CustomMarginsOrientation.BOTTOM)),
            Math.round(margins.get(CustomMarginsOrientation.LEFT)));
      }
      print_preview.ticket_items.TicketItem.prototype.updateValue.call(
          this, margins);
    },

    /**
     * Updates the specified margin in points while keeping the value within
     * a maximum and minimum.
     * @param {!print_preview.ticket_items.CustomMarginsOrientation}
     *     orientation Specifies the margin to update.
     * @param {number} value Updated margin value in points.
     */
    updateMargin: function(orientation, value) {
      var margins = /** @type {!print_preview.Margins} */ (this.getValue());
      var oppositeOrientation = CustomMargins.OppositeOrientation_[orientation];
      var max =
          this.getMarginMax_(orientation, margins.get(oppositeOrientation));
      value = Math.max(0, Math.min(max, value));
      this.updateValue(margins.set(orientation, value));
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.getDocumentInfoInternal().margins ||
          new print_preview.Margins(72, 72, 72, 72);
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return this.getDocumentInfoInternal().margins ||
          new print_preview.Margins(72, 72, 72, 72);
    },

    /**
     * @param {!print_preview.ticket_items.CustomMarginsOrientation}
     *     orientation Specifies which margin to get the maximum value of.
     * @param {number} oppositeMargin Value of the margin in points
     *     opposite the specified margin.
     * @return {number} Maximum value in points of the specified margin.
     * @private
     */
    getMarginMax_: function(orientation, oppositeMargin) {
      var dimensionLength = (orientation == CustomMarginsOrientation.TOP ||
                             orientation == CustomMarginsOrientation.BOTTOM) ?
          this.getDocumentInfoInternal().pageSize.height :
          this.getDocumentInfoInternal().pageSize.width;

      var totalMargin =
          dimensionLength - CustomMargins.MINIMUM_MARGINS_DISTANCE_;
      return Math.round(totalMargin > 0 ? totalMargin - oppositeMargin : 0);
    }
  };

  // Export
  return {CustomMargins: CustomMargins};
});
