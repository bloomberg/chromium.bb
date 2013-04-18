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
      var cdd = this.capabilitiesHolder_.get();
      if (!cdd || !cdd.printer || !cdd.printer.color) {
        return false;
      }
      var hasStandardColor = false;
      var hasStandardMonochrome = false;
      cdd.printer.color.option.forEach(function(option) {
        hasStandardColor = hasStandardColor || option.type == 'STANDARD_COLOR';
        hasStandardMonochrome = hasStandardMonochrome ||
                                option.type == 'STANDARD_MONOCHROME';
      });
      return hasStandardColor && hasStandardMonochrome;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var cdd = this.capabilitiesHolder_.get();
      var defaultOption = this.getDefaultColorOption_(cdd.printer.color.option);
      return defaultOption && defaultOption.type == 'STANDARD_COLOR';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      var cdd = this.capabilitiesHolder_.get();
      var defaultOption = null;
      if (cdd && cdd.printer && cdd.printer.color) {
        defaultOption = this.getDefaultColorOption_(cdd.printer.color.option);
      }

      // TODO(rltoscano): Get rid of this check based on destination ID. These
      // destinations should really update their CDDs to have only one color
      // option that has type 'STANDARD_COLOR'.
      var dest = this.destinationStore_.selectedDestination;
      if (!dest) {
        return false;
      }
      return dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
          dest.id == print_preview.Destination.GooglePromotedId.FEDEX ||
          dest.type == print_preview.Destination.Type.MOBILE ||
          defaultOption && defaultOption.type == 'STANDARD_COLOR';
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
