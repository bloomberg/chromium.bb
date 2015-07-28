// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  /** Amount that each level of bookmarks is indented by (px). */
  var BOOKMARK_INDENT = 20;

  Polymer({
    is: 'viewer-bookmark',

    properties: {
      /**
       * A bookmark object, each containing a:
       * - title
       * - page (optional)
       * - children (an array of bookmarks)
       */
      bookmark: {
        type: Object,
        observer: 'bookmarkChanged_'
      },

      depth: {
        type: Number,
        observer: 'depthChanged'
      },

      childDepth: Number,

      childrenShown_: {
        type: Boolean,
        value: false
      }
    },

    bookmarkChanged_: function() {
      this.$.expand.style.visibility =
          this.bookmark.children.length > 0 ? 'visible' : 'hidden';
    },

    depthChanged: function() {
      this.childDepth = this.depth + 1;
      this.$.item.style.paddingLeft = (this.depth * BOOKMARK_INDENT) + 'px';
    },

    onClick: function() {
      if (this.bookmark.hasOwnProperty('page'))
        this.fire('change-page', {page: this.bookmark.page});
    },

    toggleChildren: function(e) {
      this.childrenShown_ = !this.childrenShown_;
      if (this.childrenShown_)
        this.$.expand.classList.add('open');
      else
        this.$.expand.classList.remove('open');
      e.stopPropagation();  // Prevent the above onClick handler from firing.
    }
  });
})();
