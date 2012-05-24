// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Duplex ticket item whose value is a {@code boolean} that indicates whether
   * the document should be duplex printed.
   * @param {!print_preview.CapabilitiesHolder} capabilitiesHolder Capabilities
   *     holder used to determine the default duplex value and if duplexing
   *     is available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Duplex(capabilitiesHolder) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Capabilities holder used to determine the default duplex value and if
     * duplexing is available.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = capabilitiesHolder;
  };

  Duplex.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.capabilitiesHolder_.get().hasDuplexCapability;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.capabilitiesHolder_.get().defaultIsDuplexEnabled;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    Duplex: Duplex
  };
});
