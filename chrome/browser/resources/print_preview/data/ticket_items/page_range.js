// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Page range ticket item whose value is a {@code string} that represents
   * which pages in the document should be printed.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function PageRange(documentInfo) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;
  };

  /**
   * Impossibly large page number.
   * @type {number}
   * @const
   * @private
   */
  PageRange.MAX_PAGE_NUMBER_ = 1000000000;

  PageRange.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return null != pageRangeTextToPageRanges(value,
                                               this.documentInfo_.pageCount);
    },

    /**
     * @return {!print_preview.PageNumberSet} Set of page numbers defined by the
     *     page range string.
     */
    getPageNumberSet: function() {
      var pageNumberList = pageRangeTextToPageList(
          this.getValue(), this.documentInfo_.pageCount);
      return new print_preview.PageNumberSet(pageNumberList);
    },

    /** @override */
    isCapabilityAvailable: function() {
      return true;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return '';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '';
    },

    /**
     * @return {!Array.<object.<{from: number, to: number}>>} A list of page
     *     ranges.
     */
    getPageRanges: function() {
      var page_ranges = pageRangeTextToPageRanges(this.getValue());
      return page_ranges ? page_ranges : [];
    },

    /**
     * @return {!Array.<object.<{from: number, to: number}>>} A list of page
     *     ranges suitable for use in the native layer.
     * TODO(vitalybuka): this should be removed when native layer switched to
     *     page ranges.
     */
    getDocumentPageRanges: function() {
      var page_ranges = pageRangeTextToPageRanges(this.getValue(),
                                                  this.documentInfo_.pageCount);
      return page_ranges ? page_ranges : [];
    },
  };

  // Export
  return {
    PageRange: PageRange
  };
});
