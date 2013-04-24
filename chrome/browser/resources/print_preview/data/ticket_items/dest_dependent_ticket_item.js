// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Ticket item that depends on the currently selected print destination.
   * @param {!print_preview.DestinationStore} destinationStore Used listen for
   *     changes in the currently selected destination's capabilities.
   * @param {print_preview.AppState=} appState Application state model to update
   *     when ticket items update.
   * @param {print_preview.AppState.Field=} field Field of the app state to
   *     update when ticket item is updated.
   */
  function DestDependentTicketItem(destinationStore, appState, field) {
    print_preview.ticket_items.TicketItem.call(this, appState, field);

    /**
     * Used listen for changes in the currently selected destination's
     * capabilities.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    this.addEventHandlers_();
  };

  DestDependentTicketItem.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /**
     * @return {print_preview.Destination} Selected destination from the
     *     destination store, or {@code null} if no destination is selected.
     * @protected
     */
    getSelectedDestInternal: function() {
      return this.destinationStore_.selectedDestination;
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

  return {
    DestDependentTicketItem: DestDependentTicketItem
  };
});
