// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Collate ticket item whose value is a {@code boolean} that indicates whether
   * collation is enabled.
   * @param {!print_preview.AppState} appState App state used to persist collate
   *     selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used determine if a destination has the collate capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Collate(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this, appState, print_preview.AppState.Field.IS_COLLATE_ENABLED);

    /**
     * Destination store used determine if a destination has the collate
     * capability.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    // TODO(rltoscano): Move DestinationStore into a base class.
    this.destinationStore_ = destinationStore;

    this.addEventHandlers_();
  };

  Collate.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !!this.getCollateCapability_();
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.getCollateCapability_().default || false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    },

    /**
     * @return {Object} Collate capability of the selected destination.
     * @private
     */
    getCollateCapability_: function() {
      var dest = this.destinationStore_.selectedDestination;
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.collate) ||
             null;
    },

    /**
     * Adds event handlers for this class.
     * @private
     */
    addEventHandlers_: function() {
      this.getTrackerInternal().add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.onCapsReady_.bind(this));
    },

    /**
     * Called when the selected destination's capabilities are ready. Dispatches
     * a CHANGE event.
     * @private
     */
    onCapsReady_: function() {
      cr.dispatchSimpleEvent(
          this, print_preview.ticket_items.TicketItem.EventType.CHANGE);
    }
  };

  // Export
  return {
    Collate: Collate
  };
});
