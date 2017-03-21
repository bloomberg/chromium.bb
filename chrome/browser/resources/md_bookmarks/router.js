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

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /**
     * Parameter q is routed to the searchTerm.
     * Parameter id is routed to the selectedId.
     * @private
     */
    queryParams_: Object,

    /** @private */
    searchTerm_: String,

    /** @private {?string} */
    selectedId_: String,
  },

  observers: [
    'onQueryChanged_(queryParams_.q)',
    'onFolderChanged_(queryParams_.id)',
    'onStateChanged_(searchTerm_, selectedId_)',
  ],

  attached: function() {
    this.watch('selectedId_', function(state) {
      return state.selectedFolder;
    });
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
  },

  /** @private */
  onQueryChanged_: function() {
    var searchTerm = this.queryParams_.q || '';
    if (searchTerm && searchTerm != this.searchTerm_) {
      this.searchTerm_ = searchTerm;
      this.dispatch(bookmarks.actions.setSearchTerm(searchTerm));
    }
  },

  /** @private */
  onFolderChanged_: function() {
    var selectedId = this.queryParams_.id;
    if (selectedId && selectedId != this.selectedId_) {
      this.selectedId_ = selectedId;
      this.dispatch(bookmarks.actions.selectFolder(selectedId));
    }
  },

  /** @private */
  onStateChanged_: function() {
    this.debounce('updateQueryParams', this.updateQueryParams_.bind(this));
  },

  /** @private */
  updateQueryParams_: function() {
    if (this.searchTerm_)
      this.queryParams_ = {q: this.searchTerm_};
    else
      this.queryParams_ = {id: this.selectedId_};
  },
});
