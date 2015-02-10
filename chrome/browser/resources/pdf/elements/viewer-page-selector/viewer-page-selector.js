// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DIGIT_LENGTH = 0.6;

Polymer('viewer-page-selector', {
  /**
   * @type {string}
   * The current entry in the input field (1-based).
   */
  pageNo: '1',

  /**
   * @type {number}
   * The index of the current page being viewed (0-based).
   */
  index: 0,

  /**
   * @type {number}
   * The number of pages the document contains.
   */
  docLength: 1,

  /**
   * @type {string}
   * The submitted input from the user (1-based).
   */
  chosenPageNo: '1',

  chosenPageNoChanged: function() {
    var page = parseInt(this.chosenPageNo);
    if (!isNaN(page)) {
      this.fire('change-page', {page: page - 1});
    } else {
      // Repopulate the input.
      this.indexChanged();
      // Change the chosenPageNo so if it is '' again we can repopulate it.
      this.chosenPageNo = this.pageNo;
    }
  },

  indexChanged: function() {
    this.pageNo = String(this.index + 1);
  },

  docLengthChanged: function() {
    var numDigits = this.docLength.toString().length;
    this.$.pageselector.style.width = (numDigits * DIGIT_LENGTH) + 'em';
  },

  select: function() {
    this.$.input.select();
  }
});
