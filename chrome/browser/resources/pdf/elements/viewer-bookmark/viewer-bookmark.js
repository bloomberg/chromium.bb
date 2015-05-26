// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-bookmark',

  properties: {
    /**
     * A bookmark object, each containing a:
     * - title
     * - page (optional)
     * - children (an array of bookmarks)
     */
    bookmark: Object
  },

  onClick: function() {
    if (this.bookmark.hasOwnProperty('page'))
      this.fire('change-page', {page: this.bookmark.page});
  },
});
