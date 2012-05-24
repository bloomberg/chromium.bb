// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DestinationStore() {
    cr.EventTarget.call(this);

    /**
     * Internal backing store for the data store.
     * @type {!Array.<print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Currently selected destination.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Initial destination ID used to auto-select the first inserted destination
     * that matches. If {@code null}, the first destination inserted into the
     * store will be selected.
     * @type {?string}
     * @private
     */
    this.initialDestinationId_ = null;

    /**
     * Whether the destination store will auto select the destination that
     * matches the initial destination.
     * @type {boolean}
     * @private
     */
    this.isInAutoSelectMode_ = false;
  };

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  DestinationStore.EventType = {
    DESTINATIONS_INSERTED:
        'print_preview.DestinationStore.DESTINATIONS_INSERTED',
    DESTINATION_SELECT: 'print_preview.DestinationStore.DESTINATION_SELECT'
  };

  DestinationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {!Array.<!print_preview.Destination>} List of destinations in
     *     the store.
     */
    get destinations() {
      return this.destinations_.slice(0);
    },

    /**
     * @return {print_preview.Destination} The currently selected destination or
     *     {@code null} if none is selected.
     */
    get selectedDestination() {
      return this.selectedDestination_;
    },

    /**
     * Sets the initially selected destination. If any inserted destinations
     * match this ID, that destination will be automatically selected. This
     * occurs only once for every time this setter is called or if the store is
     * cleared.
     * @param {string} ID of the destination that should be selected
     *     automatically when added to the store.
     */
    setInitialDestinationId: function(initialDestinationId) {
      this.initialDestinationId_ = initialDestinationId;
      this.isInAutoSelectMode_ = true;
      if (this.initialDestinationId_ == null && this.destinations_.length > 0) {
        this.selectDestination(this.destinations_[0]);
      } else if (this.initialDestinationId_ != null) {
        for (var dest, i = 0; dest = this.destinations_[i]; i++) {
          if (dest.id == initialDestinationId) {
            this.selectDestination(dest);
            break;
          }
        }
      }
    },

    /** @param {!print_preview.Destination} Destination to select. */
    selectDestination: function(destination) {
      this.selectedDestination_ = destination;
      this.selectedDestination_.isRecent = true;
      this.isInAutoSelectMode_ = false;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
    },

    /**
     * Inserts a print destination to the data store and dispatches a
     * DESTINATIONS_INSERTED event. If the destination matches the initial
     * destination ID, then the destination will be automatically selected.
     * @param {!print_preview.Destination} destination Print destination to
     *     insert.
     */
    insertDestination: function(destination) {
      this.destinations_.push(destination);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      if (this.isInAutoSelectMode_) {
        if (this.initialDestinationId_ == null) {
          this.selectDestination(destination);
        } else {
          if (destination.id == this.initialDestinationId_) {
            this.selectDestination(destination);
          }
        }
      }
    },

    /**
     * Inserts multiple print destinations to the data store and dispatches one
     * DESTINATIONS_INSERTED event. If any of the destinations match the initial
     * destination ID, then that destination will be automatically selected.
     * @param {!Array.<print_preview.Destination>} destinations Print
     *     destinations to insert.
     */
    insertDestinations: function(destinations) {
      this.destinations_ = this.destinations_.concat(destinations);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      if (this.isInAutoSelectMode_) {
        if (this.initialDestinationId_ == null && destinations.length > 0) {
          this.selectDestination(destinations[0]);
        } else if (this.initialDestinationId_ != null) {
          for (var dest, i = 0; dest = destinations[i]; i++) {
            if (dest.id == this.initialDestinationId_) {
              this.selectDestination(dest);
              break;
            }
          }
        }
      }
    },

    /**
     * Updates an existing print destination with capabilities information. If
     * the destination doesn't already exist, it will be added.
     * @param {!print_preview.Destination} destination Destination to update.
     * @return {!print_preview.Destination} The existing destination that was
     *     updated.
     */
    updateDestination: function(destination) {
      var existingDestination = null;
      for (var d, i = 0; d = this.destinations_[i]; i++) {
        if (destination.id == d.id) {
          existingDestination = d;
          break;
        }
      }
      if (existingDestination) {
        existingDestination.capabilities = destination.capabilities;
        return existingDestination;
      } else {
        this.insertDestination(destination);
      }
    },

    /** Clears all print destinations. */
    clear: function() {
      this.destinations_ = [];
      this.selectedDestination_ = null;
      this.isInAutoSelectMode_ = true;
    }
  };

  // Export
  return {
    DestinationStore: DestinationStore
  };
});
