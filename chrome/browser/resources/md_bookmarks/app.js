// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-app',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /** @private */
    searchTerm_: {
      type: String,
      observer: 'searchTermChanged_',
    },
  },

  /** @override */
  attached: function() {
    this.watch('searchTerm_', function(store) {
      return store.search.term;
    });

    chrome.bookmarks.getTree(function(results) {
      var nodeList = bookmarks.util.normalizeNodes(results[0]);
      var initialState = bookmarks.util.createEmptyState();
      initialState.nodes = nodeList;
      initialState.selectedFolder = nodeList['0'].children[0];

      bookmarks.Store.getInstance().init(initialState);
      bookmarks.ApiListener.init();
    }.bind(this));
  },

  searchTermChanged_: function() {
    if (!this.searchTerm_)
      return;

    chrome.bookmarks.search(this.searchTerm_, function(results) {
      var ids = results.map(function(node) {
        return node.id;
      });
      this.dispatch(bookmarks.actions.setSearchResults(ids));
    }.bind(this));
  },
});
