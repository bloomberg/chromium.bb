// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Landscape ticket item whose value is a {@code boolean} that indicates
   * whether the document should be printed in landscape orientation.
   * @param {!print_preview.AppState} appState App state object used to persist
   *     ticket item values.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used to determine the default landscape value and if landscape
   *     printing is available.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.ticket_items.MarginsType} marginsType Reset when
   *     landscape value changes.
   * @param {!print_preview.ticket_items.CustomMargins} customMargins Reset when
   *     landscape value changes.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Landscape(appState, destinationStore, documentInfo, marginsType,
                     customMargins) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppStateField.IS_LANDSCAPE_ENABLED,
        destinationStore,
        documentInfo);

    /**
     * Margins ticket item. Reset when landscape ticket item changes.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ = marginsType;

    /**
     * Custom margins ticket item. Reset when landscape ticket item changes.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = customMargins;
  }

  Landscape.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      var cap = this.getPageOrientationCapability_();
      if (!cap)
        return false;
      var hasAutoOrPortraitOption = false;
      var hasLandscapeOption = false;
      cap.option.forEach(function(option) {
        hasAutoOrPortraitOption = hasAutoOrPortraitOption ||
            option.type == 'AUTO' ||
            option.type == 'PORTRAIT';
        hasLandscapeOption = hasLandscapeOption || option.type == 'LANDSCAPE';
      });
      // TODO(rltoscano): Technically, the print destination can still change
      // the orientation of the print out (at least for cloud printers) if the
      // document is not modifiable. But the preview wouldn't update in this
      // case so it would be a bad user experience.
      return this.getDocumentInfoInternal().isModifiable &&
          !this.getDocumentInfoInternal().hasCssMediaStyles &&
          hasAutoOrPortraitOption &&
          hasLandscapeOption;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var cap = this.getPageOrientationCapability_();
      var defaultOptions = cap.option.filter(function(option) {
        return option.is_default;
      });
      return defaultOptions.length == 0 ? false :
                                          defaultOptions[0].type == 'LANDSCAPE';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      var doc = this.getDocumentInfoInternal();
      return doc.hasCssMediaStyles ?
          (doc.pageSize.width > doc.pageSize.height) :
          false;
    },

    /** @override */
    updateValueInternal: function(value) {
      var updateMargins = !this.isValueEqual(value);
      print_preview.ticket_items.TicketItem.prototype.updateValueInternal.call(
          this, value);
      if (updateMargins) {
        // Reset the user set margins when page orientation changes.
        this.marginsType_.updateValue(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT);
        this.customMargins_.updateValue(null);
      }
    },

    /**
     * @return {boolean} Whether capability contains the |value|.
     * @param {string} value Option to check.
     */
    hasOption: function(value) {
      var cap = this.getPageOrientationCapability_();
      if (!cap)
        return false;
      return cap.option.some(function(option) {
        return option.type == value;
      });
    },

    /**
     * @return {Object} Page orientation capability of the selected destination.
     * @private
     */
    getPageOrientationCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.page_orientation) ||
             null;
    }
  };

  // Export
  return {
    Landscape: Landscape
  };
});
