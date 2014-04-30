// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Media size ticket item.
   * @param {!print_preview.AppState} appState App state used to persist media
   *     size selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used to determine if a destination has the media size capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function MediaSize(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.MEDIA_SIZE,
        destinationStore);
  };

  MediaSize.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      if (!this.isCapabilityAvailable()) {
        return false;
      }
      return this.capability.option.some(function(option) {
        return option.width_microns == value.width_microns &&
               option.height_microns == value.height_microns &&
               option.is_continuous_feed == value.is_continuous_feed &&
               option.vendor_id == value.vendor_id;
      });
    },

    /** @override */
    isCapabilityAvailable: function() {
      // TODO(alekseys): Turn it on when the feature is ready (crbug/239879).
      return false && !!this.capability;
    },

    /** @return {Object} Media size capability of the selected destination. */
    get capability() {
      var destination = this.getSelectedDestInternal();
      return (destination &&
              destination.capabilities &&
              destination.capabilities.printer &&
              destination.capabilities.printer.media_size) ||
             null;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var defaultOptions = this.capability.option.filter(function(option) {
        return option.is_default;
      });
      return defaultOptions.length > 0 ? defaultOptions[0] : null;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return {};
    }
  };

  // Export
  return {
    MediaSize: MediaSize
  };
});
