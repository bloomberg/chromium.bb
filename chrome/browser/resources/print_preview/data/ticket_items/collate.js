// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Collate ticket item whose value is a {@code boolean} that indicates whether
   * collation is enabled.
   * @param {!print_preview.CapabilitiesHolder} capabilitiesHolder Capabilities
   *     holder used to determine the default collate value and if the collate
   *     capability is available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Collate(capabilitiesHolder) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Capabilities holder used to determine the default collate value and if
     * the collate capability is available.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = capabilitiesHolder;
  };

  Collate.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.capabilitiesHolder_.get().hasCollateCapability;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.capabilitiesHolder_.get().defaultIsCollateEnabled;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    Collate: Collate
  };
});
