// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/** @type {!RegExp} */
const SANITIZE_REGEX = /[-[\]{}()*+?.,\\^$|#\s]/g;

Polymer({
  is: 'print-preview-search-box',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    /** @type {?RegExp} */
    searchQuery: {
      type: Object,
      notify: true,
    },
  },

  listeners: {
    'search-changed': 'onSearchChanged_',
  },

  /** @return {!HTMLInputElement} */
  getSearchInput: function() {
    return this.$.searchInput;
  },

  /**
   * @param {!CustomEvent} e Event containing the new search.
   */
  onSearchChanged_: function(e) {
    const safeQuery = e.detail.trim().replace(SANITIZE_REGEX, '\\$&');
    this.searchQuery =
        safeQuery.length > 0 ? new RegExp(`(${safeQuery})`, 'i') : null;
  },
});
})();
