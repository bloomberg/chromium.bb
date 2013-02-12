// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object which contains information related to the document to print.
   * @constructor
   */
  function DocumentInfo() {
    /**
     * Whether the document to print is modifiable (i.e. can be reflowed).
     * @type {boolean}
     */
    this.isModifiable = true;

    /**
     * Number of pages in the document to print.
     * @type {number}
     */
    this.pageCount = 0;

    /**
     * Size of the pages of the document in points.
     * @type {!print_preview.Size}
     */
    this.pageSize = new print_preview.Size(0, 0);

    /**
     * Printable area of the document in points.
     * @type {!print_preview.PrintableArea}
     */
    this.printableArea = new print_preview.PrintableArea(
        new print_preview.Coordinate2d(0, 0), new print_preview.Size(0, 0));

    /**
     * Whether the document is styled by CSS media styles.
     * @type {boolean}
     */
    this.hasCssMediaStyles = false;

    /**
     * Whether the document has selected content.
     * @type {boolean}
     */
    this.documentHasSelection = false;

    /**
     * Margins of the document in points.
     * @type {print_preview.Margins}
     */
    this.margins = null;

    /**
     * Title of document.
     * @type {string}
     */
    this.title = '';
  };

  // Export
  return {
    DocumentInfo: DocumentInfo
  };
});
