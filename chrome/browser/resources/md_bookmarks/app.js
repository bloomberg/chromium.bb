// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-app',

  properties: {
    selectedId: String,

    /** @type {BookmarkTreeNode} */
    selectedNode: Object,

    /** @type {BookmarkTreeNode} */
    rootNode: Object,
  },

  /** @override */
  attached: function() {
    /** @type {BookmarksStore} */ (this.$$('bookmarks-store'))
        .initializeStore();
  },
});
