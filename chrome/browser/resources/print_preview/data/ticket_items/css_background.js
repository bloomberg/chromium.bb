// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Ticket item whose value is a {@code boolean} that represents whether to
   * print CSS backgrounds.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function CssBackground(documentInfo) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;
  };

  CssBackground.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.documentInfo_.isModifiable;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    CssBackground: CssBackground
  };
});
