// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Ticket item whose value is a {@code boolean} that represents whether to
   * distill the page before printing.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function DistillPage(documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        null /*appState*/,
        null /*field*/,
        null /*destinationStore*/,
        documentInfo);

    this.isAvailable_ = false;
  };

  DistillPage.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.isAvailable_;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    },

    setIsCapabilityAvailable: function(isAvailable) {
      if (this.isAvailable_ == isAvailable)
        return;

      this.isAvailable_ = isAvailable;
      this.dispatchChangeEventInternal();
    }
  };

  // Export
  return {
    DistillPage: DistillPage
  };
});
