// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-sidebar',

  behaviors: [
    bookmarks.StoreClient,
  ],

  properties: {
    /** @type {Array<string>} */
    rootFolders: Array,
  },

  attached: function() {
    this.watch('rootFolders', function(store) {
      return store.nodes['0'].children;
    });
    this.updateFromStore();
  },
});
