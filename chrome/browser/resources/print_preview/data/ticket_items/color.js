// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Color ticket item whose value is a {@code boolean} that indicates whether
   * the document should be printed in color.
   * @param {!print_preview.AppState} appState App state persistence object to
   *     save the state of the color selection.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     whether color printing should be available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Color(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_COLOR_ENABLED,
        destinationStore);
  };

  Color.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      var colorCap = this.getColorCapability_();
      if (!colorCap) {
        return false;
      }
      var hasColor = false;
      var hasMonochrome = false;
      colorCap.option.forEach(function(option) {
        hasColor = hasColor || option.type == 'STANDARD_COLOR';
        hasMonochrome = hasMonochrome || option.type == 'STANDARD_MONOCHROME';
      });
      return hasColor && hasMonochrome;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var colorCap = this.getColorCapability_();
      var defaultOption = this.getDefaultColorOption_(colorCap.option);
      return defaultOption && defaultOption.type == 'STANDARD_COLOR';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      var colorCap = this.getColorCapability_();
      var defaultOption = colorCap ?
          this.getDefaultColorOption_(colorCap.option) : null;

      // TODO(rltoscano): Get rid of this check based on destination ID. These
      // destinations should really update their CDDs to have only one color
      // option that has type 'STANDARD_COLOR'.
      var dest = this.getSelectedDestInternal();
      if (!dest) {
        return false;
      }
      return dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
          dest.id == print_preview.Destination.GooglePromotedId.FEDEX ||
          dest.type == print_preview.Destination.Type.MOBILE ||
          defaultOption && defaultOption.type == 'STANDARD_COLOR';
    },

    /**
     * @return {Object} Color capability of the selected destination.
     * @private
     */
    getColorCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.color) ||
             null;
    },

    /**
     * @param options {!Array.<!Object.<{type: string=, is_default: boolean=}>>
     * @return {Object.<{type: string=, is_default: boolean=}>} Default color
     *     option of the given list.
     * @private
     */
    getDefaultColorOption_: function(options) {
      var defaultOptions = options.filter(function(option) {
        return option.is_default;
      });
      return (defaultOptions.length == 0) ? null : defaultOptions[0];
    }
  };

  // Export
  return {
    Color: Color
  };
});
