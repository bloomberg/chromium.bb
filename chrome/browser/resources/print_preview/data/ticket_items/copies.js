// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Copies ticket item whose value is a {@code string} that indicates how many
   * copies of the document should be printed. The ticket item is backed by a
   * string since the user can textually input the copies value.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used determine if a destination has the copies capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Copies(destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this, null /*appState*/, null /*field*/, destinationStore);
  }

  Copies.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return value != '';
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !!this.getCopiesCapability_();
    },

    /** @return {number} The number of copies indicated by the ticket item. */
    getValueAsNumber: function() {
      var value = this.getValue();
      return value == '' ? 0 : parseInt(value, 10);
    },

    /** @override */
    getDefaultValueInternal: function() {
      var cap = this.getCopiesCapability_();
      return cap.hasOwnProperty('default') ? cap.default : '1';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '1';
    },

    /**
     * @return {Object} Copies capability of the selected destination.
     * @private
     */
    getCopiesCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest && dest.capabilities && dest.capabilities.printer &&
              dest.capabilities.printer.copies) ||
          null;
    }
  };

  // Export
  return {Copies: Copies};
});
