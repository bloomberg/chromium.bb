// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Color ticket item whose value is a {@code boolean} that indicates whether
   * the document should be printed in color.
   * @param {!print_preview.CapabilitiesHolder} capabilitiesHolder Capabilities
   *     holder used to determine the default color value and if the color
   *     capability is available.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     whether color printing should be available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Color(capabilitiesHolder, destinationStore) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Capabilities holder used to determine the default color value and if the
     * color capability is available.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = capabilitiesHolder;

    /**
     * Used to determine whether color printing should be available.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;
  };

  Color.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.capabilitiesHolder_.get().hasColorCapability &&
          (!this.destinationStore_.selectedDestination ||
              this.destinationStore_.selectedDestination.id !=
                  print_preview.Destination.GooglePromotedId.SAVE_AS_PDF);
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.capabilitiesHolder_.get().defaultIsColorEnabled;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      var dest = this.destinationStore_.selectedDestination;
      if (!dest) {
        return false;
      }
      return dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
          dest.id == print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
          dest.id == print_preview.Destination.GooglePromotedId.FEDEX ||
          dest.type == print_preview.Destination.Type.MOBILE ||
          this.capabilitiesHolder_.get().defaultIsColorEnabled;
    }
  };

  // Export
  return {
    Color: Color
  };
});
