// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-app',

  behaviors: [
    bookmarks.StoreClient,
  ],

  /** @override */
  attached: function() {
    chrome.bookmarks.getTree(function(results) {
      var nodeList = bookmarks.util.normalizeNodes(results[0]);
      var initialState = bookmarks.util.createEmptyState();
      initialState.nodes = nodeList;
      initialState.selectedFolder = nodeList['0'].children[0];

      bookmarks.Store.getInstance().init(initialState);
      bookmarks.ApiListener.init();
    }.bind(this));
  },
});
