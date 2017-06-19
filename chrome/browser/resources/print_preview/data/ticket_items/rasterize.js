// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Rasterize ticket item whose value is a {@code boolean} that indicates
   * whether the PDF document should be rendered as images.
   * @constructor
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print, used to determine if document is a PDF.
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Rasterize(destinationStore, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this, null /* appState */, null /* field */,
        null /* destinationStore */, documentInfo);
  }

  Rasterize.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return this.getDefaultValueInternal();
    }
  };

  // Export
  return {Rasterize: Rasterize};
});
