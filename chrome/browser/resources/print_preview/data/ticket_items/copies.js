// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Copies ticket item whose value is a {@code string} that indicates how many
   * copies of the document should be printed. The ticket item is backed by a
   * string since the user can textually input the copies value.
   * @param {!print_preview.CapabilitiesHolder} capabilitiesHolder Capabilities
   *     holder used to determine the default number of copies and if the copies
   *     capability is available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Copies(capabilitiesHolder) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Capabilities holder used to determine the default number of copies and if
     * the copies capability is available.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = capabilitiesHolder;
  };

  Copies.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      if (/[^\d]+/.test(value)) {
        return false;
      }
      var copies = parseInt(value);
      if (copies > 999 || copies < 1) {
        return false;
      }
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.capabilitiesHolder_.get().hasCopiesCapability;
    },

    /** @return {number} The number of copies indicated by the ticket item. */
    getValueAsNumber: function() {
      return parseInt(this.getValue());
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.capabilitiesHolder_.get().defaultCopiesStr;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '1';
    }
  };

  // Export
  return {
    Copies: Copies
  };
});
