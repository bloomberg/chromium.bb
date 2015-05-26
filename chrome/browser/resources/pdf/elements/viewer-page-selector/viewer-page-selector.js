// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DIGIT_LENGTH = 0.6;

Polymer({
  is: 'viewer-page-selector',

  properties: {
    /**
     * The index of the current page being viewed (0-based).
     */
    index: {
      type: Number,
      value: 0,
      observer: 'indexChanged'
    },

    /**
     * The number of pages the document contains.
     */
    docLength: {
      type: Number,
      value: 1,
      observer: 'docLengthChanged'
    },

    /**
     * The current entry in the input field (1-based).
     */
    pageNo: {
      type: String,
      value: '1',
      observer: 'pageNoChanged'
    },
  },

  pageNoChanged: function() {
    var page = parseInt(this.pageNo);
    if (!isNaN(page) && page != this.index + 1) {
      this.fire('change-page', {page: page - 1});
    } else {
      // Repopulate the input.
      this.indexChanged();
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
