// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-page-selector',

  properties: {
    /**
     * The number of pages the document contains.
     */
    docLength: {
      type: Number,
      value: 1,
      observer: 'docLengthChanged'
    },

    /**
     * The current page being viewed (1-based).
     */
    pageNo: {
      type: String,
      value: '1'
    },

    strings: Object
  },

  pageNoCommitted: function() {
    var page = parseInt(this.pageNo);
    if (!isNaN(page)) {
      this.fire('change-page', {page: page - 1});
      this.$.input.blur();
    }
  },

  docLengthChanged: function() {
    var numDigits = this.docLength.toString().length;
    this.$.pageselector.style.width = numDigits + 'ch';
    // Set both sides of the slash to the same width, so that the layout is
    // exactly centered.
    this.$['pagelength-spacer'].style.width = numDigits + 'ch';
  },

  select: function() {
    this.$.input.select();
  },

  /**
   * @return {boolean} True if the selector input field is currently focused.
   */
  isActive: function() {
    return this.shadowRoot.activeElement == this.$.input;
  }
});
