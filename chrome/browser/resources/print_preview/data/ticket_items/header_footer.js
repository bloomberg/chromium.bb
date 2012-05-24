// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Header-footer ticket item whose value is a {@code boolean} that indicates
   * whether the document should be printed with headers and footers.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.ticket_items.MarginsType} marginsType Ticket item
   *     that stores which predefined margins to print with.
   * @param {!print_preview.ticket_items.CustomMargins} customMargins Ticket
   *     item that stores custom margin values.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function HeaderFooter(documentInfo, marginsType, customMargins) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Ticket item that stores which predefined margins to print with.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ = marginsType;

    /**
     * Ticket item that stores custom margin values.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = customMargins;
  };

  HeaderFooter.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      if (!this.documentInfo_.isModifiable) {
        return false;
      } else if (this.marginsType_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.NO_MARGINS) {
        return false;
      } else if (this.marginsType_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.MINIMUM) {
        return true;
      }
      var margins;
      if (this.marginsType_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        if (!this.customMargins_.isValid()) {
          return false;
        }
        margins = this.customMargins_.getValue();
      } else {
        margins = this.documentInfo_.margins;
      }
      var orientEnum = print_preview.ticket_items.CustomMargins.Orientation;
      return margins == null ||
             margins.get(orientEnum.TOP) > 0 ||
             margins.get(orientEnum.BOTTOM) > 0;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return true;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    HeaderFooter: HeaderFooter
  };
});
