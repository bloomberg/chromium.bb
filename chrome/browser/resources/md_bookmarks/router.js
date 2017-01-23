// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  /**
   * This element is a one way bound interface that routes the page URL to
   * the searchTerm and selectedId. Clients must initialize themselves by
   * reading the router's fields after attach.
   */
  is: 'bookmarks-router',

  properties: {
    // Parameter q is routed to the searchTerm.
    // Parameter id is routed to the selectedId.
    queryParams_: Object,

    searchTerm: {
      type: String,
      observer: 'onSearchTermChanged_',
    },

    /** @type {?string} */
    selectedId: {
      type: String,
      observer: 'onSelectedIdChanged_',
    },
  },

  observers: [
    'onQueryChanged_(queryParams_.*)',
  ],

  /** @private */
  onQueryChanged_: function() {
    this.searchTerm = this.queryParams_.q || '';
    this.selectedId = this.queryParams_.id;

    if (this.searchTerm)
      this.fire('search-term-changed', this.searchTerm);
    else
      this.fire('selected-folder-changed', this.selectedId);
  },

  /** @private */
  onSelectedIdChanged_: function() {
    this.set('queryParams_.id', this.selectedId || null);
  },

  /** @private */
  onSearchTermChanged_: function() {
    this.set('queryParams_.q', this.searchTerm || null);
  },
});
