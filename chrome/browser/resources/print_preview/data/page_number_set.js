// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * An immutable ordered set of page numbers.
   * @param {!Array.<number>} pageNumberList A list of page numbers to include
   *     in the set.
   * @constructor
   */
  function PageNumberSet(pageNumberList) {
    /**
     * Internal data store for the page number set.
     * @type {!Array.<number>}
     * @private
     */
    this.pageNumberSet_ = pageListToPageSet(pageNumberList);
  };

  /**
   * @param {string} pageRangeStr String form of a page range. I.e. '2,3,4-5'.
   *     If string is empty, all page numbers will be in the page number set.
   * @param {number} totalPageCount Total number of pages in the original
   *     document.
   * @return {print_preview.PageNumberSet} Page number set parsed from the
   *     given page range string and total page count. Null returned if
   *     the given page range string is invalid.
   */
  PageNumberSet.parse = function(pageRangeStr, totalPageCount) {
    if (pageRangeStr == '') {
      var pageNumberList = [];
      for (var i = 0; i < totalPageCount; i++) {
        pageNumberList.push(i + 1);
      }
      return new PageNumberSet(pageNumberList);
    } else {
      return isPageRangeTextValid(pageRangeStr, totalPageCount) ?
          new PageNumberSet(
              pageRangeTextToPageList(pageRangeStr, totalPageCount)) : null;
    }
  };

  PageNumberSet.prototype = {
    /** @return {number} The number of page numbers in the set. */
    get size() {
      return this.pageNumberSet_.length;
    },

    /**
     * @param {number} index 0-based index of the page number to get.
     * @return {number} Page number at the given index.
     */
    getPageNumberAt: function(index) {
      return this.pageNumberSet_[index];
    },

    /**
     * @param {number} 1-based page number to check for.
     * @return {boolean} Whether the given page number is in the page range.
     */
    hasPageNumber: function(pageNumber) {
      return arrayContains(this.pageNumberSet_, pageNumber);
    },

    /**
     * @param {number} 1-based number of the page to get index of.
     * @return {number} 0-based index of the given page number with respect to
     *     all of the pages in the page range.
     */
    getPageNumberIndex: function(pageNumber) {
      return this.pageNumberSet_.indexOf(pageNumber);
    },

    /**
     * @return {!Array.<object.<{from: number, to: number}>>} A list of page
     *     ranges suitable for use in the native layer.
     */
    getPageRanges: function() {
      return pageSetToPageRanges(this.pageNumberSet_);
    },

    /** @return {!Array.<number>} Array representation of the set. */
    asArray: function() {
      return this.pageNumberSet_.slice(0);
    },

    /**
     * @param {print_preview.PageNumberSet} other Page number set to compare
     *     against.
     * @return {boolean} Whether another page number set is equal to this one.
     */
    equals: function(other) {
      return other == null ?
          false : areArraysEqual(this.pageNumberSet_, other.pageNumberSet_);
    }
  };

  // Export
  return {
    PageNumberSet: PageNumberSet
  };
});
