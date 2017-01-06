// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'bookmarks-item',

  properties: {
    /** @type {BookmarkTreeNode} */
    item: {
      type: Object,
      observer: 'onItemChanged_',
    },

    isFolder_: Boolean,
  },

  observers: [
    'updateFavicon_(item.url)',
  ],

  /**
   * @param {Event} e
   * @private
   */
  onMenuButtonOpenTap_: function(e) {
    this.fire('open-item-menu', {
      target: e.target,
      item: this.item
    });
  },

  /** @private */
  onItemChanged_: function() {
    this.isFolder_ = !(this.item.url);
  },

  /** @private */
  updateFavicon_: function(url) {
    this.$.icon.style.backgroundImage = cr.icon.getFavicon(url);
  },
});
