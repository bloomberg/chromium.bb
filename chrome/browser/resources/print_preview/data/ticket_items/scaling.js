// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Scaling ticket item whose value is a {@code string} that indicates what the
   * scaling (in percent) of the document should be. The ticket item is backed
   * by a string since the user can textually input the scaling value.
   * @param {!print_preview.AppState} appState App state to persist item value.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     whether fit to page should be available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Scaling(appState, destinationStore, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this, appState, print_preview.AppStateField.SCALING, destinationStore,
        documentInfo);
  }

  Scaling.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isValueEqual: function(value) {
      return this.getValueAsNumber() == value;
    },

    /** @override */
    isCapabilityAvailable: function() {
      // This is not a function of the printer, but should be disabled if we are
      // saving a PDF to a PDF.
      var knownSizeToSaveAsPdf =
          (!this.getDocumentInfoInternal().isModifiable ||
           this.getDocumentInfoInternal().hasCssMediaStyles) &&
          this.getSelectedDestInternal() &&
          this.getSelectedDestInternal().id ==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
      return !knownSizeToSaveAsPdf;
    },

    /** @return {number} The scaling percentage indicated by the ticket item. */
    getValueAsNumber: function() {
      return parseInt(this.getValue(), 10);
    },

    /** @override */
    getDefaultValueInternal: function() {
      return '100';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '100';
    },
  };

  // Export
  return {Scaling: Scaling};
});
