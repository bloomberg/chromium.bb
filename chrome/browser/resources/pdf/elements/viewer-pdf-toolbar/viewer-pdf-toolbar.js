// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-pdf-toolbar', {
  /**
   * @type {string}
   * The title of the PDF document.
   */
  docTitle: '',

  /**
   * @type {number}
   * The current index of the page being viewed (0-based).
   */
  pageIndex: 0,

  /**
   * @type {number}
   * The current loading progress of the PDF document (0 - 100).
   */
  loadProgress: 0,

  /**
   * @type {number}
   * The number of pages in the PDF document.
   */
  docLength: 1,

  loadProgressChanged: function() {
    if (this.loadProgress >= 100)
      this.$.pageselector.style.visibility = 'visible';
  },

  selectPageNumber: function() {
    this.$.pageselector.select();
  },

  rotateRight: function() {
    this.fire('rotate-right');
  },

  toggleBookmarks: function() {
    this.fire('toggle-bookmarks');
  },

  save: function() {
    this.fire('save');
  },

  print: function() {
    this.fire('print');
  }
});
