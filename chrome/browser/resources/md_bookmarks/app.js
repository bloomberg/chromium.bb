// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-app',

  properties: {
    selectedId: String,

    /** @type {BookmarkTreeNode} */
    rootNode: Object,

    searchTerm: String,

    /** @type {Array<BookmarkTreeNode>} */
    displayedList: Array,
  },

  /** @override */
  attached: function() {
    /** @type {BookmarksStore} */ (this.$$('bookmarks-store'))
        .initializeStore();
  },
});
